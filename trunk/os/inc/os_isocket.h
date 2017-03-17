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
#define SOCKET_ERROR (-1)
#define closesocket(s) close(s)
#endif

#if defined(_OS_WINDOWS)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#undef EWOULDBLOCK
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

namespace os {

class IPAddress {
 public:
  IPAddress();
  explicit IPAddress(const in_addr& ip4);
  explicit IPAddress(const in6_addr& ip6);
  explicit IPAddress(uint32_t ip_in_host_byte_order);
  IPAddress(const IPAddress& other);

  virtual ~IPAddress() {}

  const IPAddress & operator=(const IPAddress& other);
  bool operator==(const IPAddress& other) const;
  bool operator!=(const IPAddress& other) const;
  bool operator <(const IPAddress& other) const;
  bool operator >(const IPAddress& other) const;
  friend std::ostream& operator<<(std::ostream& os, const IPAddress& addr);

  int32_t family() const;
  in_addr ipv4_address() const;
  in6_addr ipv6_address() const;
  int32_t addr_size() const;

  std::string to_string() const;
  std::string to_sensitive_string() const;
  IPAddress normalized() const;
  IPAddress as_ipv6_address() const;

  uint32_t ipv4_address_host() const;
  bool is_invalid() const;

 private:
  int32_t _family;
  union {
    in_addr ip4;
    in6_addr ip6;
  } _u;
};

class SocketAddress {
 public:
  SocketAddress();
  SocketAddress(const std::string& hostname, int32_t port);
  SocketAddress(uint32_t ip_as_host_order_integer, int32_t port);
  SocketAddress(const IPAddress& ip, int32_t port);
  SocketAddress(const SocketAddress& addr);
  void Clear();
  bool IsNil() const;
  bool IsComplete() const;
  SocketAddress& operator=(const SocketAddress& addr);
  void set_ip(uint32_t ip_as_host_order_integer);
  void set_ip(const IPAddress& ip);
  void set_ip(const std::string& hostname);
  void set_resolved_ip(uint32_t ip_as_host_order_integer);
  void set_resolved_ip(const IPAddress& ip);
  void set_port(int32_t port);

  const std::string& hostname() const { return _hostname; }
  uint32_t ip() const;
  uint16_t port() const;
  const IPAddress& ip_addr() const;
  int32_t family() const { return _ip.family(); }

  void set_scope_id(int32_t id) { _scope_id = id; }
  int32_t scope_id() const {return _scope_id; }

  std::string host_as_uri_string() const;
  std::string host_as_sensitive_uri_string() const;
  std::string port_as_string() const;
  std::string to_string() const;
  std::string to_sensitive_string() const;
  bool from_string(const std::string& str);
  friend std::ostream& operator<<(std::ostream& os, const SocketAddress& addr);

  bool is_any_ip() const;
  bool is_loopback_ip() const;
  bool is_private_ip() const;
  bool is_unresolved_ip() const;

  bool operator ==(const SocketAddress& addr) const;
  inline bool operator !=(const SocketAddress& addr) const {
    return !this->operator ==(addr);
  }

  bool operator <(const SocketAddress& addr) const;

  bool equal_ips(const SocketAddress& addr) const;
  bool equal_ports(const SocketAddress& addr) const;
  int32_t hash() const;

  void to_sock_addr(sockaddr_in* saddr) const;
  bool from_sock_addr(const sockaddr_in& saddr);

  int32_t to_dual_stack_sock_addr_storage(sockaddr_storage* saddr) const;
  int32_t to_sock_addr_storage(sockaddr_storage* saddr) const;

 private:
  std::string _hostname;
  IPAddress _ip;
  uint16_t _port;
  int32_t _scope_id;
  bool _literal;
};

class ISocket {
 public:
  virtual ~ISocket() {}

  static ISocket *Create(int32_t family, int32_t type, int32_t protocol);

  virtual SocketAddress local_address() const = 0;
  virtual SocketAddress remote_address() const = 0;

  virtual int32_t bind(const SocketAddress& addr) = 0;
  virtual int32_t connect(const SocketAddress& addr) = 0;
  virtual int32_t send(const void *pv, int32_t cb) = 0;
  virtual int32_t send_to(const void *pv, int32_t cb, const SocketAddress& addr) = 0;
  virtual int32_t recv(void* pv, int32_t cb, int64_t* timestamp) = 0;
  virtual int32_t recv_from(void* pv,
                       int32_t cb,
                       SocketAddress* paddr,
                       int64_t* timestamp) = 0;
  virtual int32_t listen(int32_t backlog) = 0;
  virtual ISocket* accept(SocketAddress *paddr) = 0;
  virtual int32_t close() = 0;
  virtual int32_t error() const = 0;
  virtual void set_error(int32_t error) = 0;
  virtual bool is_blocking() const  = 0;

  enum ConnState {
    CS_CLOSED,
    CS_CONNECTING,
    CS_CONNECTED
  };
  virtual ConnState state() const = 0;
  virtual int32_t estimate_mtu(uint16_t* mtu) = 0;

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
  virtual int32_t get_option(Option opt, int32_t* value) = 0;
  virtual int32_t set_option(Option opt, int32_t value) = 0;

 protected:
  ISocket() {}
};

}  // namespace os

#endif  // _OS_SOCKET_H__
