#include <os_assert.h>
#include <os_isocket_impl.h>

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

#if defined(_OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#undef set_port
#endif

#include <algorithm>
#include <map>

#if defined(_OS_POSIX)
#include <netinet/tcp.h>  // for TCP_NODELAY
#define IP_MTU 14 // Until this is integrated from linux/in.h to netinet/in.h
typedef void* SockOptArg;
#endif  // _OS_POSIX

#if defined(_OS_POSIX) && !defined(_OS_MAC) && !defined(__native_client__)
int64_t GetSocketRecvTimestamp(int32_t socket) {
  struct timeval tv_ioctl;
  int32_t ret = ioctl(socket, SIOCGSTAMP, &tv_ioctl);
  if (ret != 0)
    return -1;
  int64_t timestamp =
      1000 * static_cast<int64_t>(tv_ioctl.tv_sec) +
      static_cast<int64_t>(tv_ioctl.tv_usec);
  return timestamp;
}
#else
int64_t GetSocketRecvTimestamp(int32_t socket) {
  return -1;
}
#endif

#if defined(_OS_WINDOWS)
typedef char* SockOptArg;
#endif

inline bool IsBlockingError(int32_t e) {
  return (e == EWOULDBLOCK) || (e == EAGAIN) || (e == EINPROGRESS);
}

inline uint32_t hton32(uint32_t n) {
  uint32_t result = 0;
  result = htonl(n);
  return result;
}
inline uint16_t hton16(uint16_t n) {
  uint16_t result = 0;
  result = htons(n);
  return result;
}
inline uint32_t ntoh32(uint32_t n) {
  uint32_t result = 0;
  result = ntohl(n);
  return result;
}

inline uint16_t ntoh16(uint16_t n) {
  uint16_t result = 0;
  result = ntohs(n);
  return result;
}

namespace os {
#if defined(_OS_WINDOWS)
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

static const int32_t IP_HEADER_SIZE = 20u;
static const int32_t IPV6_HEADER_SIZE = 40u;
static const int32_t ICMP_HEADER_SIZE = 8u;
static const int32_t ICMP_PING_TIMEOUT_MILLIS = 10000u;
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
                         ntoh16(saddr->sin_port));
    return true;
  } else if (addr.ss_family == AF_INET6) {
    const sockaddr_in6* saddr = reinterpret_cast<const sockaddr_in6*>(&addr);
    logv("ip: %d port: %d\n", saddr->sin6_addr, saddr->sin6_port);
    *out = SocketAddress(IPAddress(saddr->sin6_addr),
                         ntoh16(saddr->sin6_port));
    out->set_scope_id(saddr->sin6_scope_id);
    return true;
  }
  return false;
}

SocketAddress EmptySocketAddressWithFamily(int32_t family) {
  if (family == AF_INET) {
    return SocketAddress(IPAddress(INADDR_ANY), 0);
  } else if (family == AF_INET6) {
    return SocketAddress(IPAddress(in6addr_any), 0);
  }
  return SocketAddress();
}

ISocket *ISocket::Create(int32_t family, int32_t type, int32_t protocol) {
  SocketImpl *ps = new SocketImpl();
  CHECK_EQ(true, ps->Create(family, type, protocol));
  return ps;
}

SocketImpl::SocketImpl(SOCKET s)
  : s_(s), enabled_events_(0), error_(0),
    state_(CS_CLOSED) {
#if defined(_OS_WINDOWS)
  // EnsureWinsockInit() ensures that winsock is initialized. The default
  // version of this function doesn't do anything because winsock is
  // initialized by constructor of a static object. If neccessary libjingle
  // users can link it with a different version of this function by replacing
  // win32socketinit.cc. See win32socketinit.cc for more details.
  EnsureWinsockInit();
#endif
  if (s_ != INVALID_SOCKET) {
    enabled_events_ = DE_READ | DE_WRITE;

    int32_t type = SOCK_STREAM;
    socklen_t len = sizeof(type);
    CHECK(0 == getsockopt(s_, SOL_SOCKET, SO_TYPE, (SockOptArg)&type, &len));
    udp_ = (SOCK_DGRAM == type);
  }

  crit_ = Mutex::Create();
}

SocketImpl::~SocketImpl() {
  if (crit_) {
    delete crit_;
    crit_ = NULL;
  }
  close();
}

bool SocketImpl::Create(int32_t family, int32_t type, int32_t protocol) {
  close();
  s_ = ::socket(family, type, protocol);
  udp_ = (SOCK_DGRAM == type);
  UpdateLastError();
  if (udp_)
    enabled_events_ = DE_READ | DE_WRITE;
  return s_ != INVALID_SOCKET;
}

SocketAddress SocketImpl::local_address() const {
  sockaddr_storage addr_storage = {0};
  socklen_t addrlen = sizeof(addr_storage);
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  int32_t result = ::getsockname(s_, addr, &addrlen);
  SocketAddress address;
  if (result >= 0) {
    SocketAddressFromSockAddrStorage(addr_storage, &address);
  } else {
    logv("local_address: unable to get local addr, socket=\n");
  }
  return address;
}

SocketAddress SocketImpl::remote_address() const {
  sockaddr_storage addr_storage = {0};
  socklen_t addrlen = sizeof(addr_storage);
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  int32_t result = ::getpeername(s_, addr, &addrlen);
  SocketAddress address;
  if (result >= 0) {
    SocketAddressFromSockAddrStorage(addr_storage, &address);
  } else {
    logv("remote_address: unable to get remote addr, socket=\n");
  }
  return address;
}

int32_t SocketImpl::bind(const SocketAddress& bind_addr) {
  sockaddr_storage addr_storage;
  int32_t len = bind_addr.to_sock_addr_storage(&addr_storage);
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);

  int32_t err = ::bind(s_, addr, static_cast<int32_t>(len));
  UpdateLastError();
#if !defined(NDEBUG)
  if (0 == err) {
    dbg_addr_ = "Bound @ ";
    dbg_addr_.append(local_address().to_string());
  }
#endif
  return err;
}

int32_t SocketImpl::connect(const SocketAddress& addr) {
  // TODO(pthatcher): Implicit creation is required to reconnect...
  // ...but should we make it more explicit?
  if (state_ != CS_CLOSED) {
    set_error(EALREADY);
    return SOCKET_ERROR;
  }
  return DoConnect(addr);
}

int32_t SocketImpl::DoConnect(const SocketAddress& connect_addr) {
  if ((s_ == INVALID_SOCKET) &&
      !Create(connect_addr.family(), SOCK_STREAM, IPPROTO_TCP)) {
    return SOCKET_ERROR;
  }
  sockaddr_storage addr_storage;
  int32_t len = connect_addr.to_sock_addr_storage(&addr_storage);
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  int32_t err = ::connect(s_, addr, static_cast<int32_t>(len));
  UpdateLastError();
  if (err == 0) {
    state_ = CS_CONNECTED;
  } else if (IsBlockingError(error())) {
    state_ = CS_CONNECTING;
    enabled_events_ |= DE_CONNECT;
  } else {
    return SOCKET_ERROR;
  }

  enabled_events_ |= DE_READ | DE_WRITE;
  return 0;
}

int32_t SocketImpl::error() const {
  AutoLock cs(crit_);
  return error_;
}

void SocketImpl::set_error(int32_t error) {
  AutoLock cs(crit_);
  error_ = error;
}

bool SocketImpl::is_blocking() const {
  AutoLock cs(crit_);
  return IsBlockingError(error());
}

ISocket::ConnState SocketImpl::state() const {
  return state_;
}

int32_t SocketImpl::get_option(Option opt, int32_t* value) {
  int32_t slevel;
  int32_t sopt;
  if (TranslateOption(opt, &slevel, &sopt) == -1)
    return -1;
  socklen_t optlen = sizeof(*value);
  int32_t ret = ::getsockopt(s_, slevel, sopt, (SockOptArg)value, &optlen);
  if (ret != -1 && opt == OPT_DONTFRAGMENT) {
#if defined(_OS_LINUX) && !defined(_OS_ANDROID)
    *value = (*value != IP_PMTUDISC_DONT) ? 1 : 0;
#endif
  }
  return ret;
}

int32_t SocketImpl::set_option(Option opt, int32_t value) {
  int32_t slevel;
  int32_t sopt;
  if (TranslateOption(opt, &slevel, &sopt) == -1)
    return -1;
  if (opt == OPT_DONTFRAGMENT) {
#if defined(_OS_LINUX) && !defined(_OS_ANDROID)
    value = (value) ? IP_PMTUDISC_DO : IP_PMTUDISC_DONT;
#endif
  }
  return ::setsockopt(s_, slevel, sopt, (SockOptArg)&value, sizeof(value));
}

int32_t SocketImpl::send(const void* pv, int32_t cb) {
  int32_t sent = DoSend(s_, reinterpret_cast<const char *>(pv),
      static_cast<int32_t>(cb),
#if defined(_OS_LINUX) && !defined(_OS_ANDROID)
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
  CHECK(sent <= static_cast<int32_t>(cb));
  if ((sent > 0 && sent < static_cast<int32_t>(cb)) ||
      (sent < 0 && IsBlockingError(error()))) {
    enabled_events_ |= DE_WRITE;
  }
  return sent;
}

int32_t SocketImpl::send_to(const void* buffer,
                           int32_t length,
                           const SocketAddress& addr) {
  sockaddr_storage saddr;
  int32_t len = addr.to_sock_addr_storage(&saddr);
  int32_t sent = DoSendTo(
      s_, static_cast<const char *>(buffer), static_cast<int32_t>(length),
#if defined(_OS_LINUX) && !defined(_OS_ANDROID)
      // Suppress SIGPIPE. See above for explanation.
      MSG_NOSIGNAL,
#else
      0,
#endif
      reinterpret_cast<sockaddr*>(&saddr), static_cast<int32_t>(len));
  UpdateLastError();
  MaybeRemapSendError();
  // We have seen minidumps where this may be false.
  CHECK(sent <= static_cast<int32_t>(length));
  if ((sent > 0 && sent < static_cast<int32_t>(length)) ||
      (sent < 0 && IsBlockingError(error()))) {
    enabled_events_ |= DE_WRITE;
  }
  return sent;
}

int32_t SocketImpl::recv(void* buffer, int32_t length, int64_t* timestamp) {
  int32_t received = ::recv(s_, static_cast<char*>(buffer),
                        static_cast<int32_t>(length), 0);
  if ((received == 0) && (length != 0)) {
    // Note: on graceful shutdown, recv can return 0.  In this case, we
    // pretend it is blocking, and then signal close, so that simplifying
    // assumptions can be made about Recv.
    logv("EOF from socket; deferring close event\n");
    // Must turn this back on so that the select() loop will notice the close
    // event.
    enabled_events_ |= DE_READ;
    set_error(EWOULDBLOCK);
    return SOCKET_ERROR;
  }
  if (timestamp) {
    *timestamp = GetSocketRecvTimestamp(s_);
  }
  UpdateLastError();
  int32_t err = error();
  bool success = (received >= 0) || IsBlockingError(err);
  if (udp_ || success) {
    enabled_events_ |= DE_READ;
  }
  if (!success) {
    loge("Error = %d\n", err);
  }
  return received;
}

int32_t SocketImpl::recv_from(void* buffer,
                             int32_t length,
                             SocketAddress* out_addr,
                             int64_t* timestamp) {
  sockaddr_storage addr_storage;
  socklen_t addr_len = sizeof(addr_storage);
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  int32_t received = ::recvfrom(s_, static_cast<char*>(buffer),
                            static_cast<int32_t>(length), 0, addr, &addr_len);
  if (timestamp) {
    *timestamp = GetSocketRecvTimestamp(s_);
  }
  UpdateLastError();
  if ((received >= 0) && (out_addr != nullptr))
    SocketAddressFromSockAddrStorage(addr_storage, out_addr);
  int32_t err = error();
  bool success = (received >= 0) || IsBlockingError(err);
  if (udp_ || success) {
    enabled_events_ |= DE_READ;
  }
  if (!success) {
    loge("Error = %d", err);
  }
  return received;
}

int32_t SocketImpl::listen(int32_t backlog) {
  int32_t err = ::listen(s_, backlog);
  UpdateLastError();
  if (err == 0) {
    state_ = CS_CONNECTING;
    enabled_events_ |= DE_ACCEPT;
#if !defined(NDEBUG)
    dbg_addr_ = "Listening @ ";
    dbg_addr_.append(local_address().to_string());
#endif
  }

  return err;
}

ISocket* SocketImpl::accept(SocketAddress* out_addr) {
  // Always re-subscribe DE_ACCEPT to make sure new incoming connections will
  // trigger an event even if DoAccept returns an error here.
  enabled_events_ |= DE_ACCEPT;
  sockaddr_storage addr_storage;
  socklen_t addr_len = sizeof(addr_storage);
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  SOCKET s = DoAccept(s_, addr, &addr_len);
  SocketAddressFromSockAddrStorage(addr_storage, out_addr);
  //UpdateLastError();
  if (s == INVALID_SOCKET)
    return NULL;
  return new SocketImpl(s);
}

int32_t SocketImpl::close() {
  if (s_ == INVALID_SOCKET)
    return 0;
  int32_t err = ::closesocket(s_);
  UpdateLastError();
  s_ = INVALID_SOCKET;
  state_ = CS_CLOSED;
  enabled_events_ = 0;
  return err;
}

int32_t SocketImpl::estimate_mtu(uint16_t* mtu) {
  SocketAddress addr = remote_address();
  if (addr.is_any_ip()) {
    set_error(ENOTCONN);
    return -1;
  }

#if defined(_OS_WINDOWS)
  // Gets the interface MTU (TTL=1) for the interface used to reach |addr|.
  WinPing ping;
  if (!ping.IsValid()) {
    set_error(EINVAL);  // can't think of a better error ID
    return -1;
  }
  int32_t header_size = ICMP_HEADER_SIZE;
  if (addr.family() == AF_INET6) {
    header_size += IPV6_HEADER_SIZE;
  } else if (addr.family() == AF_INET) {
    header_size += IP_HEADER_SIZE;
  }

  for (int32_t level = 0; PACKET_MAXIMUMS[level + 1] > 0; ++level) {
    int32_t size = PACKET_MAXIMUMS[level] - header_size;
    WinPing::PingResult result = ping.Ping(addr.ip_addr(), size,
                                           ICMP_PING_TIMEOUT_MILLIS,
                                           1, false);
    if (result == WinPing::PING_FAIL) {
      set_error(EINVAL);  // can't think of a better error ID
      return -1;
    } else if (result != WinPing::PING_TOO_LARGE) {
      *mtu = PACKET_MAXIMUMS[level];
      return 0;
    }
  }

  CHECK(false);
  return -1;
#elif defined(_OS_MAC)
  // No simple way to do this on Mac OS X.
  // SIOCGIFMTU would work if we knew which interface would be used, but
  // figuring that out is pretty complicated. For now we'll return an error
  // and let the caller pick a default MTU.
  set_error(EINVAL);
  return -1;
#elif defined(_OS_LINUX)
  // Gets the path MTU.
  int32_t value;
  socklen_t vlen = sizeof(value);
  int32_t err = getsockopt(s_, IPPROTO_IP, IP_MTU, &value, &vlen);
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

SOCKET SocketImpl::DoAccept(SOCKET socket,
                                sockaddr* addr,
                                socklen_t* addrlen) {
  return ::accept(socket, addr, addrlen);
}

int32_t SocketImpl::DoSend(SOCKET socket, const char* buf, int32_t len, int32_t flags) {
  return ::send(socket, buf, len, flags);
}

int32_t SocketImpl::DoSendTo(SOCKET socket,
                             const char* buf,
                             int32_t len,
                             int32_t flags,
                             const struct sockaddr* dest_addr,
                             socklen_t addrlen) {
  return ::sendto(socket, buf, len, flags, dest_addr, addrlen);
}

void SocketImpl::UpdateLastError() {
#ifdef _OS_POSIX
  set_error(errno);
#endif
}

void SocketImpl::MaybeRemapSendError() {
#if defined(_OS_MAC)
  // https://developer.apple.com/library/mac/documentation/Darwin/
  // Reference/ManPages/man2/sendto.2.html
  // ENOBUFS - The output queue for a network interface is full.
  // This generally indicates that the interface has stopped sending,
  // but may be caused by transient congestion.
  if (error() == ENOBUFS) {
    set_error(EWOULDBLOCK);
  }
#endif
}

int32_t SocketImpl::TranslateOption(Option opt, int32_t* slevel, int32_t* sopt) {
  switch (opt) {
    case OPT_DONTFRAGMENT:
#if defined(_OS_WINDOWS)
      *slevel = IPPROTO_IP;
      *sopt = IP_DONTFRAGMENT;
      break;
#elif defined(_OS_MAC) || defined(BSD) || defined(__native_client__)
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
IPAddress::IPAddress() : _family(AF_UNSPEC) {
    ::memset(&_u, 0, sizeof(_u));
  }

IPAddress::IPAddress(const in_addr& ip4) : _family(AF_INET) {
    memset(&_u, 0, sizeof(_u));
    _u.ip4 = ip4;
  }

IPAddress::IPAddress(const in6_addr& ip6) : _family(AF_INET6) {
    _u.ip6 = ip6;
  }

IPAddress::IPAddress(uint32_t ip_in_host_byte_order) : _family(AF_INET) {
    memset(&_u, 0, sizeof(_u));
    _u.ip4.s_addr = hton32(ip_in_host_byte_order);
  }

IPAddress::IPAddress(const IPAddress& other) : _family(other._family) {
    ::memcpy(&_u, &other._u, sizeof(_u));
  }


const IPAddress & IPAddress::operator=(const IPAddress& other) {
    _family = other._family;
    ::memcpy(&_u, &other._u, sizeof(_u));
    return *this;
  }


uint32_t IPAddress::ipv4_address_host() const {
  if (_family == AF_INET) {
    return ntoh32(_u.ip4.s_addr);
  } else {
    return 0;
  }
}

bool IPIsUnspec(const IPAddress& ip) {
  return ip.family() == AF_UNSPEC;
}

bool IPAddress::is_invalid() const {
  return IPIsUnspec(*this);
}

int32_t IPAddress::addr_size() const {
  switch (_family) {
    case AF_INET:
      return sizeof(in_addr);
    case AF_INET6:
      return sizeof(in6_addr);
  }
  return 0;
}


bool IPAddress::operator==(const IPAddress &other) const {
  if (_family != other._family) {
    return false;
  }
  if (_family == AF_INET) {
    return memcmp(&_u.ip4, &other._u.ip4, sizeof(_u.ip4)) == 0;
  }
  if (_family == AF_INET6) {
    return memcmp(&_u.ip6, &other._u.ip6, sizeof(_u.ip6)) == 0;
  }
  return _family == AF_UNSPEC;
}

bool IPAddress::operator!=(const IPAddress &other) const {
  return !((*this) == other);
}

bool IPAddress::operator >(const IPAddress &other) const {
  return (*this) != other && !((*this) < other);
}

bool IPAddress::operator <(const IPAddress &other) const {
  // IPv4 is 'less than' IPv6
  if (_family != other._family) {
    if (_family == AF_UNSPEC) {
      return true;
    }
    if (_family == AF_INET && other._family == AF_INET6) {
      return true;
    }
    return false;
  }
  // Comparing addresses of the same family.
  switch (_family) {
    case AF_INET: {
      return ntoh32(_u.ip4.s_addr) <
          ntoh32(other._u.ip4.s_addr);
    }
    case AF_INET6: {
      return memcmp(&_u.ip6.s6_addr, &other._u.ip6.s6_addr, 16) < 0;
    }
  }
  // Catches AF_UNSPEC and invalid addresses.
  return false;
}

std::ostream& operator<<(std::ostream& os, const IPAddress& ip) {
  os << ip.to_string();
  return os;
}

in6_addr IPAddress::ipv6_address() const {
  return _u.ip6;
}

in_addr IPAddress::ipv4_address() const {
  return _u.ip4;
}

int32_t IPAddress::family() const { return _family; }

const char* inet_ntop(int32_t af, const void *src, char* dst, socklen_t size) {
#if defined(_OS_WINDOWS)
  return win32_inet_ntop(af, src, dst, size);
#else
  return ::inet_ntop(af, src, dst, size);
#endif
}

int32_t inet_pton(int32_t af, const char* src, void *dst) {
#if defined(_OS_WINDOWS)
  return win32_inet_pton(af, src, dst);
#else
  return ::inet_pton(af, src, dst);
#endif
}
std::string IPAddress::to_string() const {
  if (_family != AF_INET && _family != AF_INET6) {
    return std::string();
  }
  char buf[INET6_ADDRSTRLEN] = {0};
  const void* src = &_u.ip4;
  if (_family == AF_INET6) {
    src = &_u.ip6;
  }
  if (!inet_ntop(_family, src, buf, sizeof(buf))) {
    return std::string();
  }
  return std::string(buf);
}

std::string IPAddress::to_sensitive_string() const {
#if !defined(NDEBUG)
  // Return non-stripped in debug.
  return to_string();
#else
  switch (_family) {
    case AF_INET: {
      std::string address = to_string();
      int32_t find_pos = address.rfind('.');
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
      int32_t len =
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

bool IPIsHelper(const IPAddress& ip, const in6_addr& tomatch, int32_t length) {
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

IPAddress IPAddress::normalized() const {
  if (_family != AF_INET6) {
    return *this;
  }
  if (!IPIsV4Mapped(*this)) {
    return *this;
  }
  in_addr addr = ExtractMappedAddress(_u.ip6);
  return IPAddress(addr);
}

IPAddress IPAddress::as_ipv6_address() const {
  if (_family != AF_INET) {
    return *this;
  }
  in6_addr v6addr = kV4MappedPrefix;
  ::memcpy(&v6addr.s6_addr[12], &_u.ip4.s_addr, sizeof(_u.ip4.s_addr));
  return IPAddress(v6addr);
}

// Socket Address
SocketAddress::SocketAddress() {
  Clear();
}

SocketAddress::SocketAddress(const std::string& hostname, int32_t port) {
  set_ip(hostname);
  set_port(port);
}

SocketAddress::SocketAddress(uint32_t ip_as_host_order_integer, int port) {
  set_ip(IPAddress(ip_as_host_order_integer));
  set_port(port);
}

SocketAddress::SocketAddress(const IPAddress& ip, int32_t port) {
  set_ip(ip);
  set_port(port);
}

SocketAddress::SocketAddress(const SocketAddress& addr) {
  this->operator=(addr);
}

void SocketAddress::Clear() {
  _hostname.clear();
  _literal = false;
  _ip = IPAddress();
  _port = 0;
  _scope_id = 0;
}

bool SocketAddress::IsNil() const {
  return _hostname.empty() && IPIsUnspec(_ip) && 0 == _port;
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
  return (!IPIsAny(_ip)) && (0 != _port);
}

SocketAddress& SocketAddress::operator=(const SocketAddress& addr) {
  _hostname = addr._hostname;
  _ip = addr._ip;
  _port = addr._port;
  _literal = addr._literal;
  _scope_id = addr._scope_id;
  return *this;
}

void SocketAddress::set_ip(uint32_t ip_as_host_order_integer) {
  _hostname.clear();
  _literal = false;
  _ip = IPAddress(ip_as_host_order_integer);
  _scope_id = 0;
}

void SocketAddress::set_ip(const IPAddress& ip) {
  _hostname.clear();
  _literal = false;
  _ip = ip;
  _scope_id = 0;
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

void SocketAddress::set_ip(const std::string& hostname) {
  _hostname = hostname;
  _literal = IPFromString(hostname, &_ip);
  if (!_literal) {
    _ip = IPAddress();
  }
  _scope_id = 0;
}

void SocketAddress::set_resolved_ip(uint32_t ip_as_host_order_integer) {
  _ip = IPAddress(ip_as_host_order_integer);
  _scope_id = 0;
}

void SocketAddress::set_resolved_ip(const IPAddress& ip) {
  _ip = ip;
  _scope_id = 0;
}

void SocketAddress::set_port(int32_t port) {
  CHECK((0 <= port) && (port < 65536));
  _port = static_cast<uint16_t>(port);
}

uint32_t SocketAddress::ip() const {
  return _ip.ipv4_address_host();
}

const IPAddress& SocketAddress::ip_addr() const {
  return _ip;
}

uint16_t SocketAddress::port() const {
  return _port;
}

std::string SocketAddress::host_as_uri_string() const {
  // If the hostname was a literal IP string, it may need to have square
  // brackets added (for SocketAddress::to_string()).
  if (!_literal && !_hostname.empty())
    return _hostname;
  if (_ip.family() == AF_INET6) {
    return "[" + _ip.to_string() + "]";
  } else {
    return _ip.to_string();
  }
}

std::string SocketAddress::host_as_sensitive_uri_string() const {
  // If the hostname was a literal IP string, it may need to have square
  // brackets added (for SocketAddress::to_string()).
  if (!_literal && !_hostname.empty())
    return _hostname;
  if (_ip.family() == AF_INET6) {
    return "[" + _ip.to_sensitive_string() + "]";
  } else {
    return _ip.to_sensitive_string();
  }
}

std::string SocketAddress::port_as_string() const {
  std::ostringstream ost;
  ost << _port;
  return ost.str();
}

std::string SocketAddress::to_string() const {
  std::ostringstream ost;
  ost << *this;
  return ost.str();
}

std::string SocketAddress::to_sensitive_string() const {
  std::ostringstream ost;
  ost << host_as_sensitive_uri_string() << ":" << port();
  return ost.str();
}

bool SocketAddress::from_string(const std::string& str) {
  if (str.at(0) == '[') {
    std::string::size_type closebracket = str.rfind(']');
    if (closebracket != std::string::npos) {
      std::string::size_type colon = str.find(':', closebracket);
      if (colon != std::string::npos && colon > closebracket) {
        set_port(strtoul(str.substr(colon + 1).c_str(), NULL, 10));
        set_ip(str.substr(1, closebracket - 1));
      } else {
        return false;
      }
    }
  } else {
    std::string::size_type pos = str.find(':');
    if (std::string::npos == pos)
      return false;
    set_port(strtoul(str.substr(pos + 1).c_str(), NULL, 10));
    set_ip(str.substr(0, pos));
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const SocketAddress& addr) {
  os << addr.host_as_uri_string() << ":" << addr.port();
  return os;
}

bool SocketAddress::is_any_ip() const {
  return IPIsAny(_ip);
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
bool SocketAddress::is_loopback_ip() const {
  return IPIsLoopback(_ip) || (IPIsAny(_ip) &&
                               0 == strcmp(_hostname.c_str(), "localhost"));
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
      return IsPrivateV4(ip.ipv4_address_host());
    }
    case AF_INET6: {
      return IPIsLinkLocal(ip) || IPIsLoopback(ip);
    }
  }
  return false;
}

bool SocketAddress::is_private_ip() const {
  return IPIsPrivate(_ip);
}

bool SocketAddress::is_unresolved_ip() const {
  return IPIsUnspec(_ip) && !_literal && !_hostname.empty();
}

bool SocketAddress::operator==(const SocketAddress& addr) const {
  return equal_ips(addr) && equal_ports(addr);
}

bool SocketAddress::operator<(const SocketAddress& addr) const {
  if (_ip != addr._ip)
    return _ip < addr._ip;

  // We only check hostnames if both IPs are ANY or unspecified.  This matches
  // equal_ips().
  if ((IPIsAny(_ip) || IPIsUnspec(_ip)) && _hostname != addr._hostname)
    return _hostname < addr._hostname;

  return _port < addr._port;
}

bool SocketAddress::equal_ips(const SocketAddress& addr) const {
  return (_ip == addr._ip) &&
      ((!IPIsAny(_ip) && !IPIsUnspec(_ip)) || (_hostname == addr._hostname));
}

bool SocketAddress::equal_ports(const SocketAddress& addr) const {
  return (_port == addr._port);
}

int32_t HashIP(const IPAddress& ip) {
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

int32_t SocketAddress::hash() const {
  int32_t h = 0;
  h ^= HashIP(_ip);
  h ^= _port | (_port << 16);
  return h;
}

void SocketAddress::to_sock_addr(sockaddr_in* saddr) const {
  memset(saddr, 0, sizeof(*saddr));
  if (_ip.family() != AF_INET) {
    saddr->sin_family = AF_UNSPEC;
    return;
  }
  saddr->sin_family = AF_INET;
  saddr->sin_port = hton16(_port);
  if (IPIsAny(_ip)) {
    saddr->sin_addr.s_addr = INADDR_ANY;
  } else {
    saddr->sin_addr = _ip.ipv4_address();
  }
}

bool SocketAddress::from_sock_addr(const sockaddr_in& saddr) {
  if (saddr.sin_family != AF_INET)
    return false;
  set_ip(ntoh32(saddr.sin_addr.s_addr));
  set_port(ntoh16(saddr.sin_port));
  _literal = false;
  return true;
}

static int32_t to_sock_addr_storage_helper(sockaddr_storage* addr,
                                      IPAddress ip,
                                      uint16_t port,
                                      int32_t scope_id) {
  memset(addr, 0, sizeof(sockaddr_storage));
  addr->ss_family = static_cast<unsigned short>(ip.family());
  if (addr->ss_family == AF_INET6) {
    sockaddr_in6* saddr = reinterpret_cast<sockaddr_in6*>(addr);
    saddr->sin6_addr = ip.ipv6_address();
    saddr->sin6_port = hton16(port);
    saddr->sin6_scope_id = scope_id;
    return sizeof(sockaddr_in6);
  } else if (addr->ss_family == AF_INET) {
    sockaddr_in* saddr = reinterpret_cast<sockaddr_in*>(addr);
    saddr->sin_addr = ip.ipv4_address();
    saddr->sin_port = hton16(port);
    return sizeof(sockaddr_in);
  }
  return 0;
}

int32_t SocketAddress::to_dual_stack_sock_addr_storage(sockaddr_storage *addr) const {
  return to_sock_addr_storage_helper(addr, _ip.as_ipv6_address(), _port, _scope_id);
}

int32_t SocketAddress::to_sock_addr_storage(sockaddr_storage* addr) const {
  return to_sock_addr_storage_helper(addr, _ip, _port, _scope_id);
}
}  // namespace os
