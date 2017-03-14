#ifndef _OS_SOCKET_H_
#define _OS_SOCKET_H_

#include <os_typedefs.h>

namespace os {

typedef uint32_t size_t;

#define SS_MAXSIZE 128
#define SS_ALIGNSIZE (sizeof (uint64_t))
#define SS_PAD1SIZE  (SS_ALIGNSIZE - sizeof(int16_t))
#define SS_PAD2SIZE  (SS_MAXSIZE - (sizeof(int16_t) + SS_PAD1SIZE +\
                                    SS_ALIGNSIZE))

// BSD requires use of HAVE_STRUCT_SOCKADDR_SA_LEN
struct socket_addr_in {
  // sin_family should be either AF_INET (IPv4) or AF_INET6 (IPv6)
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
  int8_t      sin_length;
  int8_t      sin_family;
#else
  int16_t     sin_family;
#endif
  uint16_t    sin_port;
  uint32_t    sin_addr;
  int8_t      sin_zero[8];
};

struct version6_in_address {
  union {
    uint8_t     _s6_u8[16];
    uint32_t    _s6_u32[4];
    uint64_t    _s6_u64[2];
  } version6_address_union;
};

struct socket_addr_in_version6 {
  // sin_family should be either AF_INET (IPv4) or AF_INET6 (IPv6)
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
  int8_t      sin_length;
  int8_t      sin_family;
#else
  int16_t     sin_family;
#endif
  // Transport layer port number.
  uint16_t sin6_port;
  // IPv6 traffic class and flow info or ip4 address.
  uint32_t sin6_flowinfo;
  // IPv6 address
  struct version6_in_address sin6_addr;
  // Set of interfaces for a scope.
  uint32_t sin6_scope_id;
};

struct socket_addr_storage {
  // sin_family should be either AF_INET (IPv4) or AF_INET6 (IPv6)
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
  int8_t   sin_length;
  int8_t   sin_family;
#else
  int16_t  sin_family;
#endif
  int8_t   __ss_pad1[SS_PAD1SIZE];
  uint64_t __ss_align;
  int8_t   __ss_pad2[SS_PAD2SIZE];
};

struct socket_addr {
  union {
    struct socket_addr_in _sockaddr_in;
    struct socket_addr_in_version6 _sockaddr_in6;
    struct socket_addr_storage _sockaddr_storage;
  };
};

typedef void* CallbackObj;
typedef void(*IncomingSocketCallback)(CallbackObj obj, const int8_t* buf,
                                      int32_t len, const socket_addr* from);

class SocketManager;

class Socket
{
public:
  static Socket * CreateSocket(const int32_t id,
      SocketManager* mgr,
      CallbackObj obj,
      IncomingSocketCallback cb,
      bool ipV6Enable = false,
      bool disableGQOS = false);

  virtual ~Socket() {}

  // Register cb for receiving callbacks when there are incoming packets.
  // Register obj so that it will be passed in calls to cb.
  virtual bool SetCallback(CallbackObj obj, IncomingSocketCallback cb) = 0;

  // Socket to local address specified by name.
  virtual bool Bind(const socket_addr& name) = 0;

  // Start receiving UDP data.
  virtual bool StartReceiving() = 0;
  // Stop receiving UDP data.
  virtual bool StopReceiving() = 0;

  virtual bool ValidHandle() = 0;

  // Set socket options.
  virtual bool SetSockopt(int32_t level, int32_t optname,
      const int8_t* optval, int32_t optlen) = 0;

  // Set TOS for outgoing packets.
  virtual int32_t SetTOS(const int32_t serviceType) = 0;

  // Set 802.1Q PCP field (802.1p) for outgoing VLAN traffic.
  virtual int32_t SetPCP(const int32_t /*pcp*/) = 0;

  // Send buf of length len to the address specified by to.
  virtual int32_t SendTo(const int8_t* buf, size_t len,
      const socket_addr& to) = 0;

  // Close socket and don't return until completed.
  virtual void CloseBlocking() {}

  // tokenRate is in bit/s. peakBandwidt is in byte/s
  virtual bool SetQos(int32_t serviceType, int32_t tokenRate,
      int32_t bucketSize, int32_t peekBandwith,
      int32_t minPolicedSize, int32_t maxSduSize,
      const socket_addr &stRemName,
      int32_t overrideDSCP = 0) = 0;

protected:
  Socket() {}
};

}  // namespace os


#endif //_OS_SOCKET_H_
