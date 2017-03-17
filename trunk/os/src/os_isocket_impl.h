#ifndef _OS_SOCKET_H_
#define _OS_SOCKET_H_

#include <memory>
#include <vector>

#include <os_mutex.h>
#include <os_isocket.h>

#if defined(_OS_POSIX)
typedef int32_t SOCKET;
#endif // _OS_POSIX

namespace os {

// Event constants for the Dispatcher class.
enum DispatcherEvent {
  DE_READ    = 0x0001,
  DE_WRITE   = 0x0002,
  DE_CONNECT = 0x0004,
  DE_CLOSE   = 0x0008,
  DE_ACCEPT  = 0x0010,
};

class SocketImpl : public ISocket {
 public:
  SocketImpl(SOCKET s = INVALID_SOCKET);
  ~SocketImpl() override;

  // Creates the underlying OS socket (same as the "socket" function).
  virtual bool Create(int32_t family, int32_t type, int32_t protocol);

  SocketAddress local_address() const override;
  SocketAddress remote_address() const override;

  int32_t bind(const SocketAddress& bind_addr) override;
  int32_t connect(const SocketAddress& addr) override;

  int32_t error() const override;
  void set_error(int32_t error) override;
  bool is_blocking() const override;

  ISocket::ConnState state() const override;

  int32_t get_option(Option opt, int32_t* value) override;
  int32_t set_option(Option opt, int32_t value) override;

  int32_t send(const void* pv, int32_t cb) override;
  int32_t send_to(const void* buffer,
             int32_t length,
             const SocketAddress& addr) override;

  int32_t recv(void* buffer, int32_t length, int64_t* timestamp) override;
  int32_t recv_from(void* buffer,
               int32_t length,
               SocketAddress* out_addr,
               int64_t* timestamp) override;

  int32_t listen(int32_t backlog) override;
  ISocket* accept(SocketAddress* out_addr) override;

  int32_t close() override;

  int32_t estimate_mtu(uint16_t* mtu) override;

 protected:
  int32_t DoConnect(const SocketAddress& connect_addr);

  // Make virtual so ::accept can be overwritten in tests.
  virtual SOCKET DoAccept(SOCKET socket, sockaddr* addr, socklen_t* addrlen);

  // Make virtual so ::send can be overwritten in tests.
  virtual int32_t DoSend(SOCKET socket, const char* buf, int32_t len, int32_t flags);

  // Make virtual so ::sendto can be overwritten in tests.
  virtual int32_t DoSendTo(SOCKET socket, const char* buf, int32_t len, int32_t flags,
                       const struct sockaddr* dest_addr, socklen_t addrlen);

  void UpdateLastError();
  void MaybeRemapSendError();

  static int32_t TranslateOption(Option opt, int32_t* slevel, int32_t* sopt);

  SOCKET s_;
  uint8_t enabled_events_;
  bool udp_;
  Mutex *crit_;
  int32_t error_ GUARDED_BY(crit_);
  ISocket::ConnState state_;

#if !defined(NDEBUG)
  std::string dbg_addr_;
#endif
};

} // namespace os

#endif // _OS_SOCKET_H_
