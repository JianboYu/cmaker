#ifndef _OS_SOCKET_H__
#define _OS_SOCKET_H__

#include <errno.h>
#include <string.h>
#include <string>
#include <vector>
#include <os_typedefs.h>

#if defined(_OS_POSIX)
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define SOCKET_EACCES EACCES
#endif

#if defined(_OS_WINDOWS)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif
// Rather than converting errors into a private namespace,
// Reuse the POSIX socket api errors. Note this depends on
// Win32 compatibility.

#if defined(_OS_WINDOWS)
#undef EWOULDBLOCK  // Remove errno.h's definition for each macro below.
#define EWOULDBLOCK WSAEWOULDBLOCK
#undef EINPROGRESS
#define EINPROGRESS WSAEINPROGRESS
#undef EALREADY
#define EALREADY WSAEALREADY
#undef ENOTSOCK
#define ENOTSOCK WSAENOTSOCK
#undef EDESTADDRREQ
#define EDESTADDRREQ WSAEDESTADDRREQ
#undef EMSGSIZE
#define EMSGSIZE WSAEMSGSIZE
#undef EPROTOTYPE
#define EPROTOTYPE WSAEPROTOTYPE
#undef ENOPROTOOPT
#define ENOPROTOOPT WSAENOPROTOOPT
#undef EPROTONOSUPPORT
#define EPROTONOSUPPORT WSAEPROTONOSUPPORT
#undef ESOCKTNOSUPPORT
#define ESOCKTNOSUPPORT WSAESOCKTNOSUPPORT
#undef EOPNOTSUPP
#define EOPNOTSUPP WSAEOPNOTSUPP
#undef EPFNOSUPPORT
#define EPFNOSUPPORT WSAEPFNOSUPPORT
#undef EAFNOSUPPORT
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#undef EADDRINUSE
#define EADDRINUSE WSAEADDRINUSE
#undef EADDRNOTAVAIL
#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#undef ENETDOWN
#define ENETDOWN WSAENETDOWN
#undef ENETUNREACH
#define ENETUNREACH WSAENETUNREACH
#undef ENETRESET
#define ENETRESET WSAENETRESET
#undef ECONNABORTED
#define ECONNABORTED WSAECONNABORTED
#undef ECONNRESET
#define ECONNRESET WSAECONNRESET
#undef ENOBUFS
#define ENOBUFS WSAENOBUFS
#undef EISCONN
#define EISCONN WSAEISCONN
#undef ENOTCONN
#define ENOTCONN WSAENOTCONN
#undef ESHUTDOWN
#define ESHUTDOWN WSAESHUTDOWN
#undef ETOOMANYREFS
#define ETOOMANYREFS WSAETOOMANYREFS
#undef ETIMEDOUT
#define ETIMEDOUT WSAETIMEDOUT
#undef ECONNREFUSED
#define ECONNREFUSED WSAECONNREFUSED
#undef ELOOP
#define ELOOP WSAELOOP
#undef ENAMETOOLONG
#define ENAMETOOLONG WSAENAMETOOLONG
#undef EHOSTDOWN
#define EHOSTDOWN WSAEHOSTDOWN
#undef EHOSTUNREACH
#define EHOSTUNREACH WSAEHOSTUNREACH
#undef ENOTEMPTY
#define ENOTEMPTY WSAENOTEMPTY
#undef EPROCLIM
#define EPROCLIM WSAEPROCLIM
#undef EUSERS
#define EUSERS WSAEUSERS
#undef EDQUOT
#define EDQUOT WSAEDQUOT
#undef ESTALE
#define ESTALE WSAESTALE
#undef EREMOTE
#define EREMOTE WSAEREMOTE
#undef EACCES
#define SOCKET_EACCES WSAEACCES
#endif  // _OS_WINDOWS

#if defined(_OS_POSIX)
#define SOCKET_ERROR (-1)
#define closesocket(s) close(s)
#endif  // _OS_POSIX

inline uint32_t HostToNetwork32(uint32_t n) {
  uint32_t result = 0;
#if 0
  uint8_t *p = (uint8_t*)&result;
  *p = (n >> 24) & 0xff;
  *(p+1) = (n >> 16) & 0xff;
  *(p+2) = (n >> 8) & 0xff;
  *(p+3) = (n >> 0) & 0xff;
#else
  result = htonl(n);
#endif

  return result;
}
inline uint16_t HostToNetwork16(uint16_t n) {
  uint16_t result = 0;
#if 0
  uint8_t *p = (uint8_t*)&result;
  *(p+2) = (n >> 8) & 0xff;
  *(p+3) = (n >> 0) & 0xff;
#else
  result = htons(n);
#endif
  return result;
}
inline uint32_t NetworkToHost32(uint32_t n) {
  uint32_t result = 0;
#if 0
  uint8_t *p = (uint8_t*)&result;
  *p = (n >> 0) & 0xff;
  *(p+1) = (n >> 8) & 0xff;
  *(p+2) = (n >> 16) & 0xff;
  *(p+3) = (n >> 24) & 0xff;
#else
  result = ntohl(n);
#endif
  return result;
}

inline uint16_t NetworkToHost16(uint16_t n) {
  uint16_t result = 0;
#if 0
  uint8_t *p = (uint8_t*)&result;
  *p = (n >> 0) & 0xff;
  *(p+1) = (n >> 8) & 0xff;
#else
  result = ntohs(n);
#endif

  return result;
}
namespace os {

inline bool IsBlockingError(int e) {
  return (e == EWOULDBLOCK) || (e == EAGAIN) || (e == EINPROGRESS);
}

struct SentPacket {
  SentPacket() : packet_id(-1), send_time_ms(-1) {}
  SentPacket(int packet_id, int64_t send_time_ms)
      : packet_id(packet_id), send_time_ms(send_time_ms) {}

  int packet_id;
  int64_t send_time_ms;
};

// Version-agnostic IP address class, wraps a union of in_addr and in6_addr.
class IPAddress {
 public:
  IPAddress() : family_(AF_UNSPEC) {
    ::memset(&u_, 0, sizeof(u_));
  }

  explicit IPAddress(const in_addr& ip4) : family_(AF_INET) {
    memset(&u_, 0, sizeof(u_));
    u_.ip4 = ip4;
  }

  explicit IPAddress(const in6_addr& ip6) : family_(AF_INET6) {
    u_.ip6 = ip6;
  }

  explicit IPAddress(uint32_t ip_in_host_byte_order) : family_(AF_INET) {
    memset(&u_, 0, sizeof(u_));
    u_.ip4.s_addr = HostToNetwork32(ip_in_host_byte_order);
  }

  IPAddress(const IPAddress& other) : family_(other.family_) {
    ::memcpy(&u_, &other.u_, sizeof(u_));
  }

  virtual ~IPAddress() {}

  const IPAddress & operator=(const IPAddress& other) {
    family_ = other.family_;
    ::memcpy(&u_, &other.u_, sizeof(u_));
    return *this;
  }

  bool operator==(const IPAddress& other) const;
  bool operator!=(const IPAddress& other) const;
  bool operator <(const IPAddress& other) const;
  bool operator >(const IPAddress& other) const;
  friend std::ostream& operator<<(std::ostream& os, const IPAddress& addr);

  int family() const { return family_; }
  in_addr ipv4_address() const;
  in6_addr ipv6_address() const;

  // Returns the number of bytes needed to store the raw address.
  size_t Size() const;

  // Wraps inet_ntop.
  std::string ToString() const;

  // Same as ToString but anonymizes it by hiding the last part.
  std::string ToSensitiveString() const;

  // Returns an unmapped address from a possibly-mapped address.
  // Returns the same address if this isn't a mapped address.
  IPAddress Normalized() const;

  // Returns this address as an IPv6 address.
  // Maps v4 addresses (as ::ffff:a.b.c.d), returns v6 addresses unchanged.
  IPAddress AsIPv6Address() const;

  // For socketaddress' benefit. Returns the IP in host byte order.
  uint32_t v4AddressAsHostOrderInteger() const;

  // Whether this is an unspecified IP address.
  bool IsNil() const;

 private:
  int family_;
  union {
    in_addr ip4;
    in6_addr ip6;
  } u_;
};


class SocketAddress {
 public:
  // Creates a nil address.
  SocketAddress();

  // Creates the address with the given host and port. Host may be a
  // literal IP string or a hostname to be resolved later.
  SocketAddress(const std::string& hostname, int port);

  // Creates the address with the given IP and port.
  // IP is given as an integer in host byte order. V4 only, to be deprecated.
  SocketAddress(uint32_t ip_as_host_order_integer, int port);

  // Creates the address with the given IP and port.
  SocketAddress(const IPAddress& ip, int port);

  // Creates a copy of the given address.
  SocketAddress(const SocketAddress& addr);

  // Resets to the nil address.
  void Clear();

  // Determines if this is a nil address (empty hostname, any IP, null port)
  bool IsNil() const;

  // Returns true if ip and port are set.
  bool IsComplete() const;

  // Replaces our address with the given one.
  SocketAddress& operator=(const SocketAddress& addr);

  // Changes the IP of this address to the given one, and clears the hostname
  // IP is given as an integer in host byte order. V4 only, to be deprecated..
  void SetIP(uint32_t ip_as_host_order_integer);

  // Changes the IP of this address to the given one, and clears the hostname.
  void SetIP(const IPAddress& ip);

  // Changes the hostname of this address to the given one.
  // Does not resolve the address; use Resolve to do so.
  void SetIP(const std::string& hostname);

  // Sets the IP address while retaining the hostname.  Useful for bypassing
  // DNS for a pre-resolved IP.
  // IP is given as an integer in host byte order. V4 only, to be deprecated.
  void SetResolvedIP(uint32_t ip_as_host_order_integer);

  // Sets the IP address while retaining the hostname.  Useful for bypassing
  // DNS for a pre-resolved IP.
  void SetResolvedIP(const IPAddress& ip);

  // Changes the port of this address to the given one.
  void SetPort(int port);

  // Returns the hostname.
  const std::string& hostname() const { return hostname_; }

  // Returns the IP address as a host byte order integer.
  // Returns 0 for non-v4 addresses.
  uint32_t ip() const;

  const IPAddress& ipaddr() const;

  int family() const {return ip_.family(); }

  // Returns the port part of this address.
  uint16_t port() const;

  // Returns the scope ID associated with this address. Scope IDs are a
  // necessary addition to IPv6 link-local addresses, with different network
  // interfaces having different scope-ids for their link-local addresses.
  // IPv4 address do not have scope_ids and sockaddr_in structures do not have
  // a field for them.
  int scope_id() const {return scope_id_; }
  void SetScopeID(int id) { scope_id_ = id; }

  // Returns the 'host' portion of the address (hostname or IP) in a form
  // suitable for use in a URI. If both IP and hostname are present, hostname
  // is preferred. IPv6 addresses are enclosed in square brackets ('[' and ']').
  std::string HostAsURIString() const;

  // Same as HostAsURIString but anonymizes IP addresses by hiding the last
  // part.
  std::string HostAsSensitiveURIString() const;

  // Returns the port as a string.
  std::string PortAsString() const;

  // Returns hostname:port or [hostname]:port.
  std::string ToString() const;

  // Same as ToString but anonymizes it by hiding the last part.
  std::string ToSensitiveString() const;

  // Parses hostname:port and [hostname]:port.
  bool FromString(const std::string& str);

  friend std::ostream& operator<<(std::ostream& os, const SocketAddress& addr);

  // Determines whether this represents a missing / any IP address.
  // That is, 0.0.0.0 or ::.
  // Hostname and/or port may be set.
  bool IsAnyIP() const;

  // Determines whether the IP address refers to a loopback address.
  // For v4 addresses this means the address is in the range 127.0.0.0/8.
  // For v6 addresses this means the address is ::1.
  bool IsLoopbackIP() const;

  // Determines whether the IP address is in one of the private ranges:
  // For v4: 127.0.0.0/8 10.0.0.0/8 192.168.0.0/16 172.16.0.0/12.
  // For v6: FE80::/16 and ::1.
  bool IsPrivateIP() const;

  // Determines whether the hostname has been resolved to an IP.
  bool IsUnresolvedIP() const;

  // Determines whether this address is identical to the given one.
  bool operator ==(const SocketAddress& addr) const;
  inline bool operator !=(const SocketAddress& addr) const {
    return !this->operator ==(addr);
  }

  // Compares based on IP and then port.
  bool operator <(const SocketAddress& addr) const;

  // Determines whether this address has the same IP as the one given.
  bool EqualIPs(const SocketAddress& addr) const;

  // Determines whether this address has the same port as the one given.
  bool EqualPorts(const SocketAddress& addr) const;

  // Hashes this address into a small number.
  size_t Hash() const;

  // Write this address to a sockaddr_in.
  // If IPv6, will zero out the sockaddr_in and sets family to AF_UNSPEC.
  void ToSockAddr(sockaddr_in* saddr) const;

  // Read this address from a sockaddr_in.
  bool FromSockAddr(const sockaddr_in& saddr);

  // Read and write the address to/from a sockaddr_storage.
  // Dual stack version always sets family to AF_INET6, and maps v4 addresses.
  // The other version doesn't map, and outputs an AF_INET address for
  // v4 or mapped addresses, and AF_INET6 addresses for others.
  // Returns the size of the sockaddr_in or sockaddr_in6 structure that is
  // written to the sockaddr_storage, or zero on failure.
  size_t ToDualStackSockAddrStorage(sockaddr_storage* saddr) const;
  size_t ToSockAddrStorage(sockaddr_storage* saddr) const;

 private:
  std::string hostname_;
  IPAddress ip_;
  uint16_t port_;
  int scope_id_;
  bool literal_;  // Indicates that 'hostname_' contains a literal IP string.
};


// General interface for the socket implementations of various networks.  The
// methods match those of normal UNIX sockets very closely.
class ISocket {
 public:
  virtual ~ISocket() {}

  static ISocket *Create(int family, int type, int protocol);

  // Returns the address to which the socket is bound.  If the socket is not
  // bound, then the any-address is returned.
  virtual SocketAddress GetLocalAddress() const = 0;

  // Returns the address to which the socket is connected.  If the socket is
  // not connected, then the any-address is returned.
  virtual SocketAddress GetRemoteAddress() const = 0;

  virtual int Bind(const SocketAddress& addr) = 0;
  virtual int Connect(const SocketAddress& addr) = 0;
  virtual int Send(const void *pv, size_t cb) = 0;
  virtual int SendTo(const void *pv, size_t cb, const SocketAddress& addr) = 0;
  virtual int Recv(void* pv, size_t cb, int64_t* timestamp) = 0;
  virtual int RecvFrom(void* pv,
                       size_t cb,
                       SocketAddress* paddr,
                       int64_t* timestamp) = 0;
  virtual int Listen(int backlog) = 0;
  virtual ISocket* Accept(SocketAddress *paddr) = 0;
  virtual int Close() = 0;
  virtual int GetError() const = 0;
  virtual void SetError(int error) = 0;
  inline bool IsBlocking() const { return IsBlockingError(GetError()); }

  enum ConnState {
    CS_CLOSED,
    CS_CONNECTING,
    CS_CONNECTED
  };
  virtual ConnState GetState() const = 0;

  // Fills in the given uint16_t with the current estimate of the MTU along the
  // path to the address to which this socket is connected. NOTE: This method
  // can block for up to 10 seconds on Windows.
  virtual int EstimateMTU(uint16_t* mtu) = 0;

  enum Option {
    OPT_DONTFRAGMENT,
    OPT_RCVBUF,      // receive buffer size
    OPT_SNDBUF,      // send buffer size
    OPT_NODELAY,     // whether Nagle algorithm is enabled
    OPT_IPV6_V6ONLY, // Whether the socket is IPv6 only.
    OPT_DSCP,        // DSCP code
    OPT_RTP_SENDTIME_EXTN_ID,  // This is a non-traditional socket option param.
                               // This is specific to libjingle and will be used
                               // if SendTime option is needed at socket level.
  };
  virtual int GetOption(Option opt, int* value) = 0;
  virtual int SetOption(Option opt, int value) = 0;

 protected:
  ISocket() {}
};

}  // namespace os

#endif  // _OS_SOCKET_H__
