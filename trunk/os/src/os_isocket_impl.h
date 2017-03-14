#ifndef _OS_SOCKET_H_
#define _OS_SOCKET_H_

#include <memory>
#include <vector>

#include <os_mutex.h>
#include <os_isocket.h>

#if defined(_OS_POSIX)
typedef int SOCKET;
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
  virtual bool Create(int family, int type, int protocol);

  SocketAddress GetLocalAddress() const override;
  SocketAddress GetRemoteAddress() const override;

  int Bind(const SocketAddress& bind_addr) override;
  int Connect(const SocketAddress& addr) override;

  int GetError() const override;
  void SetError(int error) override;

  ISocket::ConnState GetState() const override;

  int GetOption(Option opt, int* value) override;
  int SetOption(Option opt, int value) override;

  int Send(const void* pv, size_t cb) override;
  int SendTo(const void* buffer,
             size_t length,
             const SocketAddress& addr) override;

  int Recv(void* buffer, size_t length, int64_t* timestamp) override;
  int RecvFrom(void* buffer,
               size_t length,
               SocketAddress* out_addr,
               int64_t* timestamp) override;

  int Listen(int backlog) override;
  ISocket* Accept(SocketAddress* out_addr) override;

  int Close() override;

  int EstimateMTU(uint16_t* mtu) override;

 protected:
  int DoConnect(const SocketAddress& connect_addr);

  // Make virtual so ::accept can be overwritten in tests.
  virtual SOCKET DoAccept(SOCKET socket, sockaddr* addr, socklen_t* addrlen);

  // Make virtual so ::send can be overwritten in tests.
  virtual int DoSend(SOCKET socket, const char* buf, int len, int flags);

  // Make virtual so ::sendto can be overwritten in tests.
  virtual int DoSendTo(SOCKET socket, const char* buf, int len, int flags,
                       const struct sockaddr* dest_addr, socklen_t addrlen);

  void UpdateLastError();
  void MaybeRemapSendError();

  static int TranslateOption(Option opt, int* slevel, int* sopt);

  SOCKET s_;
  uint8_t enabled_events_;
  bool udp_;
  Mutex *crit_;
  int error_ GUARDED_BY(crit_);
  ISocket::ConnState state_;

#if !defined(NDEBUG)
  std::string dbg_addr_;
#endif
};

} // namespace os

#endif // _OS_SOCKET_H_
