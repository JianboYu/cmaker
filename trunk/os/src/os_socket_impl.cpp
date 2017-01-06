#include <os_assert.h>
#include <os_socket_impl.h>

#if defined(_MSC_VER) && _MSC_VER < 1300
#pragma warning(disable:4786)
#endif

#include <assert.h>

#ifdef MEMORY_SANITIZER
#include <sanitizer/msan_interface.h>
#endif

#if defined(_OS_POSIX)
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <signal.h>
#endif

#if defined(_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#undef SetPort
#endif

#include <algorithm>
#include <map>

#if defined(_OS_POSIX)
#include <netinet/tcp.h>  // for TCP_NODELAY
#define IP_MTU 14 // Until this is integrated from linux/in.h to netinet/in.h
typedef void* SockOptArg;
#endif  // _OS_POSIX

#if defined(_OS_POSIX) && !defined(_OS_MAC) && !defined(__native_client__)
int64_t GetSocketRecvTimestamp(int socket) {
  struct timeval tv_ioctl;
  int ret = ioctl(socket, SIOCGSTAMP, &tv_ioctl);
  if (ret != 0)
    return -1;
  int64_t timestamp =
      1000 * static_cast<int64_t>(tv_ioctl.tv_sec) +
      static_cast<int64_t>(tv_ioctl.tv_usec);
  return timestamp;
}
#else
int64_t GetSocketRecvTimestamp(int socket) {
  return -1;
}
#endif

#if defined(_OS_WIN)
typedef char* SockOptArg;
#endif

namespace os {
#if defined(_OS_WIN)
// Standard MTUs, from RFC 1191
const uint16_t PACKET_MAXIMUMS[] = {
    65535,  // Theoretical maximum, Hyperchannel
    32000,  // Nothing
    17914,  // 16Mb IBM Token Ring
    8166,   // IEEE 802.4
    // 4464,   // IEEE 802.5 (4Mb max)
    4352,   // FDDI
    // 2048,   // Wideband Network
    2002,   // IEEE 802.5 (4Mb recommended)
    // 1536,   // Expermental Ethernet Networks
    // 1500,   // Ethernet, Point-to-Point (default)
    1492,   // IEEE 802.3
    1006,   // SLIP, ARPANET
    // 576,    // X.25 Networks
    // 544,    // DEC IP Portal
    // 512,    // NETBIOS
    508,    // IEEE 802/Source-Rt Bridge, ARCNET
    296,    // Point-to-Point (low delay)
    68,     // Official minimum
    0,      // End of list marker
};

static const int IP_HEADER_SIZE = 20u;
static const int IPV6_HEADER_SIZE = 40u;
static const int ICMP_HEADER_SIZE = 8u;
static const int ICMP_PING_TIMEOUT_MILLIS = 10000u;
#endif

bool SocketAddressFromSockAddrStorage(const sockaddr_storage& addr,
                                      SocketAddress* out) {
  if (!out) {
    return false;
  }
  logv("addr.ss_family: %s\n", addr.ss_family == AF_INET ? "AF_INET" : "AF_INET6");
  if (addr.ss_family == AF_INET) {
    const sockaddr_in* saddr = reinterpret_cast<const sockaddr_in*>(&addr);
    *out = SocketAddress(IPAddress(saddr->sin_addr),
                         NetworkToHost16(saddr->sin_port));
    return true;
  } else if (addr.ss_family == AF_INET6) {
    const sockaddr_in6* saddr = reinterpret_cast<const sockaddr_in6*>(&addr);
    logv("ip: %d port: %d\n", saddr->sin6_addr, saddr->sin6_port);
    *out = SocketAddress(IPAddress(saddr->sin6_addr),
                         NetworkToHost16(saddr->sin6_port));
    out->SetScopeID(saddr->sin6_scope_id);
    return true;
  }
  return false;
}

SocketAddress EmptySocketAddressWithFamily(int family) {
  if (family == AF_INET) {
    return SocketAddress(IPAddress(INADDR_ANY), 0);
  } else if (family == AF_INET6) {
    return SocketAddress(IPAddress(in6addr_any), 0);
  }
  return SocketAddress();
}

Socket *Socket::Create(int family, int type) {
  PhysicalSocket *ps = new PhysicalSocket();
  CHECK_EQ(true, ps->Create(family, type));
  return ps;
}

PhysicalSocket::PhysicalSocket()
  : enabled_events_(0), error_(0),
    state_(CS_CLOSED) {
  s_ = INVALID_SOCKET;
#if defined(_OS_WIN)
  // EnsureWinsockInit() ensures that winsock is initialized. The default
  // version of this function doesn't do anything because winsock is
  // initialized by constructor of a static object. If neccessary libjingle
  // users can link it with a different version of this function by replacing
  // win32socketinit.cc. See win32socketinit.cc for more details.
  EnsureWinsockInit();
#endif
  if (s_ != INVALID_SOCKET) {
    enabled_events_ = DE_READ | DE_WRITE;

    int type = SOCK_STREAM;
    socklen_t len = sizeof(type);
    CHECK(0 == getsockopt(s_, SOL_SOCKET, SO_TYPE, (SockOptArg)&type, &len));
    udp_ = (SOCK_DGRAM == type);
  }
}

PhysicalSocket::~PhysicalSocket() {
  Close();
}

bool PhysicalSocket::Create(int family, int type) {
  Close();
  s_ = ::socket(family, type, 0);
  udp_ = (SOCK_DGRAM == type);
  UpdateLastError();
  if (udp_)
    enabled_events_ = DE_READ | DE_WRITE;
  return s_ != INVALID_SOCKET;
}

SocketAddress PhysicalSocket::GetLocalAddress() const {
  sockaddr_storage addr_storage = {0};
  socklen_t addrlen = sizeof(addr_storage);
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  int result = ::getsockname(s_, addr, &addrlen);
  SocketAddress address;
  logv("GetLocalAddress: result: %d\n", result);
  if (result >= 0) {
    SocketAddressFromSockAddrStorage(addr_storage, &address);
  } else {
    logv("GetLocalAddress: unable to get local addr, socket=\n");
  }
  return address;
}

SocketAddress PhysicalSocket::GetRemoteAddress() const {
  sockaddr_storage addr_storage = {0};
  socklen_t addrlen = sizeof(addr_storage);
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  int result = ::getpeername(s_, addr, &addrlen);
  SocketAddress address;
  if (result >= 0) {
    SocketAddressFromSockAddrStorage(addr_storage, &address);
  } else {
    logv("GetRemoteAddress: unable to get remote addr, socket=\n");
  }
  return address;
}

int PhysicalSocket::Bind(const SocketAddress& bind_addr) {
  sockaddr_storage addr_storage;
  size_t len = bind_addr.ToSockAddrStorage(&addr_storage);
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  int err = ::bind(s_, addr, static_cast<int>(len));
  UpdateLastError();
#if !defined(NDEBUG)
  if (0 == err) {
    dbg_addr_ = "Bound @ ";
    dbg_addr_.append(GetLocalAddress().ToString());
  }
#endif
  return err;
}

int PhysicalSocket::Connect(const SocketAddress& addr) {
  // TODO(pthatcher): Implicit creation is required to reconnect...
  // ...but should we make it more explicit?
  if (state_ != CS_CLOSED) {
    SetError(EALREADY);
    return SOCKET_ERROR;
  }
  return DoConnect(addr);
}

int PhysicalSocket::DoConnect(const SocketAddress& connect_addr) {
  if ((s_ == INVALID_SOCKET) &&
      !Create(connect_addr.family(), SOCK_STREAM)) {
    return SOCKET_ERROR;
  }
  sockaddr_storage addr_storage;
  size_t len = connect_addr.ToSockAddrStorage(&addr_storage);
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  int err = ::connect(s_, addr, static_cast<int>(len));
  UpdateLastError();
  if (err == 0) {
    state_ = CS_CONNECTED;
  } else if (IsBlockingError(GetError())) {
    state_ = CS_CONNECTING;
    enabled_events_ |= DE_CONNECT;
  } else {
    return SOCKET_ERROR;
  }

  enabled_events_ |= DE_READ | DE_WRITE;
  return 0;
}

int PhysicalSocket::GetError() const {
  AutoLock cs(crit_);
  return error_;
}

void PhysicalSocket::SetError(int error) {
  AutoLock cs(crit_);
  error_ = error;
}

Socket::ConnState PhysicalSocket::GetState() const {
  return state_;
}

int PhysicalSocket::GetOption(Option opt, int* value) {
  int slevel;
  int sopt;
  if (TranslateOption(opt, &slevel, &sopt) == -1)
    return -1;
  socklen_t optlen = sizeof(*value);
  int ret = ::getsockopt(s_, slevel, sopt, (SockOptArg)value, &optlen);
  if (ret != -1 && opt == OPT_DONTFRAGMENT) {
#if defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)
    *value = (*value != IP_PMTUDISC_DONT) ? 1 : 0;
#endif
  }
  return ret;
}

int PhysicalSocket::SetOption(Option opt, int value) {
  int slevel;
  int sopt;
  if (TranslateOption(opt, &slevel, &sopt) == -1)
    return -1;
  if (opt == OPT_DONTFRAGMENT) {
#if defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)
    value = (value) ? IP_PMTUDISC_DO : IP_PMTUDISC_DONT;
#endif
  }
  return ::setsockopt(s_, slevel, sopt, (SockOptArg)&value, sizeof(value));
}

int PhysicalSocket::Send(const void* pv, size_t cb) {
  int sent = DoSend(s_, reinterpret_cast<const char *>(pv),
      static_cast<int>(cb),
#if defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)
      // Suppress SIGPIPE. Without this, attempting to send on a socket whose
      // other end is closed will result in a SIGPIPE signal being raised to
      // our process, which by default will terminate the process, which we
      // don't want. By specifying this flag, we'll just get the error EPIPE
      // instead and can handle the error gracefully.
      MSG_NOSIGNAL
#else
      0
#endif
      );
  UpdateLastError();
  MaybeRemapSendError();
  // We have seen minidumps where this may be false.
  CHECK(sent <= static_cast<int>(cb));
  if ((sent > 0 && sent < static_cast<int>(cb)) ||
      (sent < 0 && IsBlockingError(GetError()))) {
    enabled_events_ |= DE_WRITE;
  }
  return sent;
}

int PhysicalSocket::SendTo(const void* buffer,
                           size_t length,
                           const SocketAddress& addr) {
  sockaddr_storage saddr;
  size_t len = addr.ToSockAddrStorage(&saddr);
  int sent = DoSendTo(
      s_, static_cast<const char *>(buffer), static_cast<int>(length),
#if defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)
      // Suppress SIGPIPE. See above for explanation.
      MSG_NOSIGNAL,
#else
      0,
#endif
      reinterpret_cast<sockaddr*>(&saddr), static_cast<int>(len));
  UpdateLastError();
  MaybeRemapSendError();
  // We have seen minidumps where this may be false.
  CHECK(sent <= static_cast<int>(length));
  if ((sent > 0 && sent < static_cast<int>(length)) ||
      (sent < 0 && IsBlockingError(GetError()))) {
    enabled_events_ |= DE_WRITE;
  }
  return sent;
}

int PhysicalSocket::Recv(void* buffer, size_t length, int64_t* timestamp) {
  int received = ::recv(s_, static_cast<char*>(buffer),
                        static_cast<int>(length), 0);
  if ((received == 0) && (length != 0)) {
    // Note: on graceful shutdown, recv can return 0.  In this case, we
    // pretend it is blocking, and then signal close, so that simplifying
    // assumptions can be made about Recv.
    logv("EOF from socket; deferring close event\n");
    // Must turn this back on so that the select() loop will notice the close
    // event.
    enabled_events_ |= DE_READ;
    SetError(EWOULDBLOCK);
    return SOCKET_ERROR;
  }
  if (timestamp) {
    *timestamp = GetSocketRecvTimestamp(s_);
  }
  UpdateLastError();
  int error = GetError();
  bool success = (received >= 0) || IsBlockingError(error);
  if (udp_ || success) {
    enabled_events_ |= DE_READ;
  }
  if (!success) {
    loge("Error = %d\n", error);
  }
  return received;
}

int PhysicalSocket::RecvFrom(void* buffer,
                             size_t length,
                             SocketAddress* out_addr,
                             int64_t* timestamp) {
  sockaddr_storage addr_storage;
  socklen_t addr_len = sizeof(addr_storage);
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  int received = ::recvfrom(s_, static_cast<char*>(buffer),
                            static_cast<int>(length), 0, addr, &addr_len);
  if (timestamp) {
    *timestamp = GetSocketRecvTimestamp(s_);
  }
  UpdateLastError();
  if ((received >= 0) && (out_addr != nullptr))
    SocketAddressFromSockAddrStorage(addr_storage, out_addr);
  int error = GetError();
  bool success = (received >= 0) || IsBlockingError(error);
  if (udp_ || success) {
    enabled_events_ |= DE_READ;
  }
  if (!success) {
    loge("Error = %d", error);
  }
  return received;
}

int PhysicalSocket::Listen(int backlog) {
  int err = ::listen(s_, backlog);
  UpdateLastError();
  if (err == 0) {
    state_ = CS_CONNECTING;
    enabled_events_ |= DE_ACCEPT;
#if !defined(NDEBUG)
    dbg_addr_ = "Listening @ ";
    dbg_addr_.append(GetLocalAddress().ToString());
#endif
  }
  return err;
}

int PhysicalSocket::Accept(SocketAddress* out_addr) {
  // Always re-subscribe DE_ACCEPT to make sure new incoming connections will
  // trigger an event even if DoAccept returns an error here.
  enabled_events_ |= DE_ACCEPT;
  sockaddr_storage addr_storage;
  socklen_t addr_len = sizeof(addr_storage);
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  SOCKET s = DoAccept(s_, addr, &addr_len);
  UpdateLastError();
  if (s == INVALID_SOCKET)
    return -1;
  return 0;
}

int PhysicalSocket::Close() {
  if (s_ == INVALID_SOCKET)
    return 0;
  int err = ::closesocket(s_);
  UpdateLastError();
  s_ = INVALID_SOCKET;
  state_ = CS_CLOSED;
  enabled_events_ = 0;
  return err;
}

int PhysicalSocket::EstimateMTU(uint16_t* mtu) {
  SocketAddress addr = GetRemoteAddress();
  if (addr.IsAnyIP()) {
    SetError(ENOTCONN);
    return -1;
  }

#if defined(_OS_WIN)
  // Gets the interface MTU (TTL=1) for the interface used to reach |addr|.
  WinPing ping;
  if (!ping.IsValid()) {
    SetError(EINVAL);  // can't think of a better error ID
    return -1;
  }
  int header_size = ICMP_HEADER_SIZE;
  if (addr.family() == AF_INET6) {
    header_size += IPV6_HEADER_SIZE;
  } else if (addr.family() == AF_INET) {
    header_size += IP_HEADER_SIZE;
  }

  for (int level = 0; PACKET_MAXIMUMS[level + 1] > 0; ++level) {
    int32_t size = PACKET_MAXIMUMS[level] - header_size;
    WinPing::PingResult result = ping.Ping(addr.ipaddr(), size,
                                           ICMP_PING_TIMEOUT_MILLIS,
                                           1, false);
    if (result == WinPing::PING_FAIL) {
      SetError(EINVAL);  // can't think of a better error ID
      return -1;
    } else if (result != WinPing::PING_TOO_LARGE) {
      *mtu = PACKET_MAXIMUMS[level];
      return 0;
    }
  }

  CHECK(false);
  return -1;
#elif defined(WEBRTC_MAC)
  // No simple way to do this on Mac OS X.
  // SIOCGIFMTU would work if we knew which interface would be used, but
  // figuring that out is pretty complicated. For now we'll return an error
  // and let the caller pick a default MTU.
  SetError(EINVAL);
  return -1;
#elif defined(WEBRTC_LINUX)
  // Gets the path MTU.
  int value;
  socklen_t vlen = sizeof(value);
  int err = getsockopt(s_, IPPROTO_IP, IP_MTU, &value, &vlen);
  if (err < 0) {
    UpdateLastError();
    return err;
  }

  CHECK((0 <= value) && (value <= 65536));
  *mtu = value;
  return 0;
#elif defined(__native_client__)
  // Most socket operations, including this, will fail in NaCl's sandbox.
  error_ = EACCES;
  return -1;
#endif
  return 0;
}

SOCKET PhysicalSocket::DoAccept(SOCKET socket,
                                sockaddr* addr,
                                socklen_t* addrlen) {
  return ::accept(socket, addr, addrlen);
}

int PhysicalSocket::DoSend(SOCKET socket, const char* buf, int len, int flags) {
  return ::send(socket, buf, len, flags);
}

int PhysicalSocket::DoSendTo(SOCKET socket,
                             const char* buf,
                             int len,
                             int flags,
                             const struct sockaddr* dest_addr,
                             socklen_t addrlen) {
  return ::sendto(socket, buf, len, flags, dest_addr, addrlen);
}

void PhysicalSocket::UpdateLastError() {
  //SetError(LAST_SYSTEM_ERROR);
}

void PhysicalSocket::MaybeRemapSendError() {
#if defined(WEBRTC_MAC)
  // https://developer.apple.com/library/mac/documentation/Darwin/
  // Reference/ManPages/man2/sendto.2.html
  // ENOBUFS - The output queue for a network interface is full.
  // This generally indicates that the interface has stopped sending,
  // but may be caused by transient congestion.
  if (GetError() == ENOBUFS) {
    SetError(EWOULDBLOCK);
  }
#endif
}

int PhysicalSocket::TranslateOption(Option opt, int* slevel, int* sopt) {
  switch (opt) {
    case OPT_DONTFRAGMENT:
#if defined(_OS_WIN)
      *slevel = IPPROTO_IP;
      *sopt = IP_DONTFRAGMENT;
      break;
#elif defined(WEBRTC_MAC) || defined(BSD) || defined(__native_client__)
      logv("Socket::OPT_DONTFRAGMENT not supported.\n");
      return -1;
#elif defined(_OS_POSIX)
      *slevel = IPPROTO_IP;
      *sopt = IP_MTU_DISCOVER;
      break;
#endif
    case OPT_RCVBUF:
      *slevel = SOL_SOCKET;
      *sopt = SO_RCVBUF;
      break;
    case OPT_SNDBUF:
      *slevel = SOL_SOCKET;
      *sopt = SO_SNDBUF;
      break;
    case OPT_NODELAY:
      *slevel = IPPROTO_TCP;
      *sopt = TCP_NODELAY;
      break;
    case OPT_DSCP:
      logv("Socket::OPT_DSCP not supported.\n");
      return -1;
    case OPT_RTP_SENDTIME_EXTN_ID:
      return -1;  // No logging is necessary as this not a OS socket option.
    default:
      CHECK(false);
      return -1;
  }
  return 0;
}

// IPAddress Cls
uint32_t IPAddress::v4AddressAsHostOrderInteger() const {
  if (family_ == AF_INET) {
    return NetworkToHost32(u_.ip4.s_addr);
  } else {
    return 0;
  }
}

bool IPIsUnspec(const IPAddress& ip) {
  return ip.family() == AF_UNSPEC;
}

bool IPAddress::IsNil() const {
  return IPIsUnspec(*this);
}

size_t IPAddress::Size() const {
  switch (family_) {
    case AF_INET:
      return sizeof(in_addr);
    case AF_INET6:
      return sizeof(in6_addr);
  }
  return 0;
}


bool IPAddress::operator==(const IPAddress &other) const {
  if (family_ != other.family_) {
    return false;
  }
  if (family_ == AF_INET) {
    return memcmp(&u_.ip4, &other.u_.ip4, sizeof(u_.ip4)) == 0;
  }
  if (family_ == AF_INET6) {
    return memcmp(&u_.ip6, &other.u_.ip6, sizeof(u_.ip6)) == 0;
  }
  return family_ == AF_UNSPEC;
}

bool IPAddress::operator!=(const IPAddress &other) const {
  return !((*this) == other);
}

bool IPAddress::operator >(const IPAddress &other) const {
  return (*this) != other && !((*this) < other);
}

bool IPAddress::operator <(const IPAddress &other) const {
  // IPv4 is 'less than' IPv6
  if (family_ != other.family_) {
    if (family_ == AF_UNSPEC) {
      return true;
    }
    if (family_ == AF_INET && other.family_ == AF_INET6) {
      return true;
    }
    return false;
  }
  // Comparing addresses of the same family.
  switch (family_) {
    case AF_INET: {
      return NetworkToHost32(u_.ip4.s_addr) <
          NetworkToHost32(other.u_.ip4.s_addr);
    }
    case AF_INET6: {
      return memcmp(&u_.ip6.s6_addr, &other.u_.ip6.s6_addr, 16) < 0;
    }
  }
  // Catches AF_UNSPEC and invalid addresses.
  return false;
}

std::ostream& operator<<(std::ostream& os, const IPAddress& ip) {
  os << ip.ToString();
  return os;
}

in6_addr IPAddress::ipv6_address() const {
  return u_.ip6;
}

in_addr IPAddress::ipv4_address() const {
  return u_.ip4;
}

const char* inet_ntop(int af, const void *src, char* dst, socklen_t size) {
#if defined(_OS_WIN)
  return win32_inet_ntop(af, src, dst, size);
#else
  return ::inet_ntop(af, src, dst, size);
#endif
}

int inet_pton(int af, const char* src, void *dst) {
#if defined(_OS_WIN)
  return win32_inet_pton(af, src, dst);
#else
  return ::inet_pton(af, src, dst);
#endif
}
std::string IPAddress::ToString() const {
  if (family_ != AF_INET && family_ != AF_INET6) {
    return std::string();
  }
  char buf[INET6_ADDRSTRLEN] = {0};
  const void* src = &u_.ip4;
  if (family_ == AF_INET6) {
    src = &u_.ip6;
  }
  if (!inet_ntop(family_, src, buf, sizeof(buf))) {
    return std::string();
  }
  return std::string(buf);
}

std::string IPAddress::ToSensitiveString() const {
#if !defined(NDEBUG)
  // Return non-stripped in debug.
  return ToString();
#else
  switch (family_) {
    case AF_INET: {
      std::string address = ToString();
      size_t find_pos = address.rfind('.');
      if (find_pos == std::string::npos)
        return std::string();
      address.resize(find_pos);
      address += ".x";
      return address;
    }
    case AF_INET6: {
      std::string result;
      result.resize(INET6_ADDRSTRLEN);
      in6_addr addr = ipv6_address();
      size_t len =
          rtc::sprintfn(&(result[0]), result.size(), "%x:%x:%x:x:x:x:x:x",
                        (addr.s6_addr[0] << 8) + addr.s6_addr[1],
                        (addr.s6_addr[2] << 8) + addr.s6_addr[3],
                        (addr.s6_addr[4] << 8) + addr.s6_addr[5]);
      result.resize(len);
      return result;
    }
  }
  return std::string();
#endif
}

in_addr ExtractMappedAddress(const in6_addr& in6) {
  in_addr ipv4;
  ::memcpy(&ipv4.s_addr, &in6.s6_addr[12], sizeof(ipv4.s_addr));
  return ipv4;
}

bool IPIsHelper(const IPAddress& ip, const in6_addr& tomatch, int length) {
  // Helper method for checking IP prefix matches (but only on whole byte
  // lengths). Length is in bits.
  in6_addr addr = ip.ipv6_address();
  return ::memcmp(&addr, &tomatch, (length >> 3)) == 0;
}

static const in6_addr kV4MappedPrefix = {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                           0xFF, 0xFF, 0}}};

bool IPIsV4Mapped(const IPAddress& ip) {
  return IPIsHelper(ip, kV4MappedPrefix, 96);
}

IPAddress IPAddress::Normalized() const {
  if (family_ != AF_INET6) {
    return *this;
  }
  if (!IPIsV4Mapped(*this)) {
    return *this;
  }
  in_addr addr = ExtractMappedAddress(u_.ip6);
  return IPAddress(addr);
}

IPAddress IPAddress::AsIPv6Address() const {
  if (family_ != AF_INET) {
    return *this;
  }
  in6_addr v6addr = kV4MappedPrefix;
  ::memcpy(&v6addr.s6_addr[12], &u_.ip4.s_addr, sizeof(u_.ip4.s_addr));
  return IPAddress(v6addr);
}

// Socket Address
SocketAddress::SocketAddress() {
  Clear();
}

SocketAddress::SocketAddress(const std::string& hostname, int port) {
  SetIP(hostname);
  SetPort(port);
}

SocketAddress::SocketAddress(uint32_t ip_as_host_order_integer, int port) {
  SetIP(IPAddress(ip_as_host_order_integer));
  SetPort(port);
}

SocketAddress::SocketAddress(const IPAddress& ip, int port) {
  SetIP(ip);
  SetPort(port);
}

SocketAddress::SocketAddress(const SocketAddress& addr) {
  this->operator=(addr);
}

void SocketAddress::Clear() {
  hostname_.clear();
  literal_ = false;
  ip_ = IPAddress();
  port_ = 0;
  scope_id_ = 0;
}

bool SocketAddress::IsNil() const {
  return hostname_.empty() && IPIsUnspec(ip_) && 0 == port_;
}

bool IPIsAny(const IPAddress& ip) {
  switch (ip.family()) {
    case AF_INET:
      return ip == IPAddress(INADDR_ANY);
    case AF_INET6:
      return ip == IPAddress(in6addr_any) || ip == IPAddress(kV4MappedPrefix);
    case AF_UNSPEC:
      return false;
  }
  return false;
}


bool SocketAddress::IsComplete() const {
  return (!IPIsAny(ip_)) && (0 != port_);
}

SocketAddress& SocketAddress::operator=(const SocketAddress& addr) {
  hostname_ = addr.hostname_;
  ip_ = addr.ip_;
  port_ = addr.port_;
  literal_ = addr.literal_;
  scope_id_ = addr.scope_id_;
  return *this;
}

void SocketAddress::SetIP(uint32_t ip_as_host_order_integer) {
  hostname_.clear();
  literal_ = false;
  ip_ = IPAddress(ip_as_host_order_integer);
  scope_id_ = 0;
}

void SocketAddress::SetIP(const IPAddress& ip) {
  hostname_.clear();
  literal_ = false;
  ip_ = ip;
  scope_id_ = 0;
}

bool IPFromString(const std::string& str, IPAddress* out) {
  if (!out) {
    return false;
  }
  in_addr addr;
  if (os::inet_pton(AF_INET, str.c_str(), &addr) == 0) {
    in6_addr addr6;
    if (os::inet_pton(AF_INET6, str.c_str(), &addr6) == 0) {
      *out = IPAddress();
      return false;
    }
    *out = IPAddress(addr6);
  } else {
    *out = IPAddress(addr);
  }
  return true;
}

void SocketAddress::SetIP(const std::string& hostname) {
  hostname_ = hostname;
  literal_ = IPFromString(hostname, &ip_);
  if (!literal_) {
    ip_ = IPAddress();
  }
  scope_id_ = 0;
}

void SocketAddress::SetResolvedIP(uint32_t ip_as_host_order_integer) {
  ip_ = IPAddress(ip_as_host_order_integer);
  scope_id_ = 0;
}

void SocketAddress::SetResolvedIP(const IPAddress& ip) {
  ip_ = ip;
  scope_id_ = 0;
}

void SocketAddress::SetPort(int port) {
  CHECK((0 <= port) && (port < 65536));
  port_ = static_cast<uint16_t>(port);
}

uint32_t SocketAddress::ip() const {
  return ip_.v4AddressAsHostOrderInteger();
}

const IPAddress& SocketAddress::ipaddr() const {
  return ip_;
}

uint16_t SocketAddress::port() const {
  return port_;
}

std::string SocketAddress::HostAsURIString() const {
  // If the hostname was a literal IP string, it may need to have square
  // brackets added (for SocketAddress::ToString()).
  if (!literal_ && !hostname_.empty())
    return hostname_;
  if (ip_.family() == AF_INET6) {
    return "[" + ip_.ToString() + "]";
  } else {
    return ip_.ToString();
  }
}

std::string SocketAddress::HostAsSensitiveURIString() const {
  // If the hostname was a literal IP string, it may need to have square
  // brackets added (for SocketAddress::ToString()).
  if (!literal_ && !hostname_.empty())
    return hostname_;
  if (ip_.family() == AF_INET6) {
    return "[" + ip_.ToSensitiveString() + "]";
  } else {
    return ip_.ToSensitiveString();
  }
}

std::string SocketAddress::PortAsString() const {
  std::ostringstream ost;
  ost << port_;
  return ost.str();
}

std::string SocketAddress::ToString() const {
  std::ostringstream ost;
  ost << *this;
  return ost.str();
}

std::string SocketAddress::ToSensitiveString() const {
  std::ostringstream ost;
  ost << HostAsSensitiveURIString() << ":" << port();
  return ost.str();
}

bool SocketAddress::FromString(const std::string& str) {
  if (str.at(0) == '[') {
    std::string::size_type closebracket = str.rfind(']');
    if (closebracket != std::string::npos) {
      std::string::size_type colon = str.find(':', closebracket);
      if (colon != std::string::npos && colon > closebracket) {
        SetPort(strtoul(str.substr(colon + 1).c_str(), NULL, 10));
        SetIP(str.substr(1, closebracket - 1));
      } else {
        return false;
      }
    }
  } else {
    std::string::size_type pos = str.find(':');
    if (std::string::npos == pos)
      return false;
    SetPort(strtoul(str.substr(pos + 1).c_str(), NULL, 10));
    SetIP(str.substr(0, pos));
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const SocketAddress& addr) {
  os << addr.HostAsURIString() << ":" << addr.port();
  return os;
}

bool SocketAddress::IsAnyIP() const {
  return IPIsAny(ip_);
}

bool IPIsLoopback(const IPAddress& ip) {
  switch (ip.family()) {
    case AF_INET: {
      return ip == IPAddress(INADDR_LOOPBACK);
    }
    case AF_INET6: {
      return ip == IPAddress(in6addr_loopback);
    }
  }
  return false;
}
bool SocketAddress::IsLoopbackIP() const {
  return IPIsLoopback(ip_) || (IPIsAny(ip_) &&
                               0 == strcmp(hostname_.c_str(), "localhost"));
}

bool IsPrivateV4(uint32_t ip_in_host_order) {
  return ((ip_in_host_order >> 24) == 127) ||
      ((ip_in_host_order >> 24) == 10) ||
      ((ip_in_host_order >> 20) == ((172 << 4) | 1)) ||
      ((ip_in_host_order >> 16) == ((192 << 8) | 168)) ||
      ((ip_in_host_order >> 16) == ((169 << 8) | 254));
}

bool IPIsLinkLocal(const IPAddress& ip) {
  // Can't use the helper because the prefix is 10 bits.
  in6_addr addr = ip.ipv6_address();
  return addr.s6_addr[0] == 0xFE && addr.s6_addr[1] == 0x80;
}

bool IPIsPrivate(const IPAddress& ip) {
  switch (ip.family()) {
    case AF_INET: {
      return IsPrivateV4(ip.v4AddressAsHostOrderInteger());
    }
    case AF_INET6: {
      return IPIsLinkLocal(ip) || IPIsLoopback(ip);
    }
  }
  return false;
}

bool SocketAddress::IsPrivateIP() const {
  return IPIsPrivate(ip_);
}

bool SocketAddress::IsUnresolvedIP() const {
  return IPIsUnspec(ip_) && !literal_ && !hostname_.empty();
}

bool SocketAddress::operator==(const SocketAddress& addr) const {
  return EqualIPs(addr) && EqualPorts(addr);
}

bool SocketAddress::operator<(const SocketAddress& addr) const {
  if (ip_ != addr.ip_)
    return ip_ < addr.ip_;

  // We only check hostnames if both IPs are ANY or unspecified.  This matches
  // EqualIPs().
  if ((IPIsAny(ip_) || IPIsUnspec(ip_)) && hostname_ != addr.hostname_)
    return hostname_ < addr.hostname_;

  return port_ < addr.port_;
}

bool SocketAddress::EqualIPs(const SocketAddress& addr) const {
  return (ip_ == addr.ip_) &&
      ((!IPIsAny(ip_) && !IPIsUnspec(ip_)) || (hostname_ == addr.hostname_));
}

bool SocketAddress::EqualPorts(const SocketAddress& addr) const {
  return (port_ == addr.port_);
}

size_t HashIP(const IPAddress& ip) {
  switch (ip.family()) {
    case AF_INET: {
      return ip.ipv4_address().s_addr;
    }
    case AF_INET6: {
      in6_addr v6addr = ip.ipv6_address();
      const uint32_t* v6_as_ints =
          reinterpret_cast<const uint32_t*>(&v6addr.s6_addr);
      return v6_as_ints[0] ^ v6_as_ints[1] ^ v6_as_ints[2] ^ v6_as_ints[3];
    }
  }
  return 0;
}

size_t SocketAddress::Hash() const {
  size_t h = 0;
  h ^= HashIP(ip_);
  h ^= port_ | (port_ << 16);
  return h;
}

void SocketAddress::ToSockAddr(sockaddr_in* saddr) const {
  memset(saddr, 0, sizeof(*saddr));
  if (ip_.family() != AF_INET) {
    saddr->sin_family = AF_UNSPEC;
    return;
  }
  saddr->sin_family = AF_INET;
  saddr->sin_port = HostToNetwork16(port_);
  if (IPIsAny(ip_)) {
    saddr->sin_addr.s_addr = INADDR_ANY;
  } else {
    saddr->sin_addr = ip_.ipv4_address();
  }
}

bool SocketAddress::FromSockAddr(const sockaddr_in& saddr) {
  if (saddr.sin_family != AF_INET)
    return false;
  SetIP(NetworkToHost32(saddr.sin_addr.s_addr));
  SetPort(NetworkToHost16(saddr.sin_port));
  literal_ = false;
  return true;
}

static size_t ToSockAddrStorageHelper(sockaddr_storage* addr,
                                      IPAddress ip,
                                      uint16_t port,
                                      int scope_id) {
  memset(addr, 0, sizeof(sockaddr_storage));
  addr->ss_family = static_cast<unsigned short>(ip.family());
  if (addr->ss_family == AF_INET6) {
    sockaddr_in6* saddr = reinterpret_cast<sockaddr_in6*>(addr);
    saddr->sin6_addr = ip.ipv6_address();
    saddr->sin6_port = HostToNetwork16(port);
    saddr->sin6_scope_id = scope_id;
    return sizeof(sockaddr_in6);
  } else if (addr->ss_family == AF_INET) {
    sockaddr_in* saddr = reinterpret_cast<sockaddr_in*>(addr);
    saddr->sin_addr = ip.ipv4_address();
    saddr->sin_port = HostToNetwork16(port);
    return sizeof(sockaddr_in);
  }
  return 0;
}

size_t SocketAddress::ToDualStackSockAddrStorage(sockaddr_storage *addr) const {
  return ToSockAddrStorageHelper(addr, ip_.AsIPv6Address(), port_, scope_id_);
}

size_t SocketAddress::ToSockAddrStorage(sockaddr_storage* addr) const {
  return ToSockAddrStorageHelper(addr, ip_, port_, scope_id_);
}
}  // namespace os
