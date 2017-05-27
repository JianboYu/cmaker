#ifndef _OS_ISOCKET_IMPL_H_
#define _OS_ISOCKET_IMPL_H_

#include <os_mutex.h>
#include <os_event.h>
#include <os_socket.h>

namespace os {

#define SOCKET_ERROR -1

class SocketPosixImpl : public Socket {
public:
  SocketPosixImpl(const int32_t id,
                   SocketManager* mgr,
                   bool ipV6Enable,
                   bool disableGQOS);
  virtual ~SocketPosixImpl();

public:
  virtual bool SetCallback(CallbackObj obj, IncomingSocketCallback cb) override;
  virtual bool Bind(const socket_addr& name) override;

  virtual bool StartReceiving() override;
  virtual bool StopReceiving() override;
  virtual bool ValidHandle() override;

  virtual bool SetSockopt(int32_t level, int32_t optname,
                          const int8_t* optval, int32_t optlen) override;
  virtual int32_t SetTOS(const int32_t serviceType) override;
  virtual int32_t SetPCP(const int32_t /*pcp*/) override;
  virtual int32_t SendTo(const int8_t* buf, int32_t len,
                         const socket_addr& to) override;

  virtual void CloseBlocking() override;
  virtual bool SetQos(int32_t serviceType, int32_t tokenRate,
                      int32_t bucketSize, int32_t peekBandwith,
                      int32_t minPolicedSize, int32_t maxSduSize,
                      const socket_addr &stRemName,
                      int32_t overrideDSCP = 0) override;

  SOCKET GetFd();
  bool CleanUp();
  void HasIncoming();
  bool WantsIncoming();
  void ReadyForDeletion();

private:
  const int32_t _id;
  IncomingSocketCallback _incomingCb;
  CallbackObj _obj;

  SOCKET _socket;
  SocketManager* _mgr;
  Event *_closeBlockingCompletedCond;
  Event *_readyForDeletionCond;

  bool _closeBlockingActive;
  bool _closeBlockingCompleted;
  bool _readyForDeletion;

  Mutex *_cs;

  bool _wantsIncoming;
};

} //namespace os

#endif //_OS_ISOCKET_IMPL_H_
