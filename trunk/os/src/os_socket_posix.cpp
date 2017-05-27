#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#define LOGTAG "SocketPosixImpl"
#include <os_log.h>
#include <os_socket_manager.h>
#include <os_socket_posix.h>

namespace os {
SocketPosixImpl::SocketPosixImpl(const int32_t id,
                                   SocketManager* mgr,
                                   bool ipV6Enable,
                                   bool /*disableGQOS*/) :
                _id(id), _mgr(mgr) {
    _wantsIncoming = false;
    _obj = NULL;
    _incomingCb = NULL;
    _readyForDeletion = false;
    _closeBlockingActive = false;
    _closeBlockingCompleted = false;
    if(ipV6Enable){
      _socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
      //_socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    }
    else {
      _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      //_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    }

    // Set socket to nonblocking mode.
    int enable_non_blocking = 1;
    if(ioctl(_socket, FIONBIO, &enable_non_blocking) == -1) {
      loge("Failed to make socket nonblocking\n");
    }
    // Enable close on fork for file descriptor so that it will not block until
    // forked process terminates.
    if(fcntl(_socket, F_SETFD, FD_CLOEXEC) == -1) {
      loge("Failed to set FD_CLOEXEC for socket\n");
    }

    _closeBlockingCompletedCond = Event::Create();
    _readyForDeletionCond = Event::Create();
    _cs = Mutex::Create();
}
SocketPosixImpl::~SocketPosixImpl() {
  if (_cs) {
    delete _cs;
    _cs = NULL;
  }
  if (_readyForDeletionCond) {
    delete _readyForDeletionCond;
    _readyForDeletionCond = NULL;
  }
  if (_closeBlockingCompletedCond) {
    delete _closeBlockingCompletedCond;
    _closeBlockingCompletedCond = NULL;
  }
  if(_socket != INVALID_SOCKET) {
    close(_socket);
    _socket = INVALID_SOCKET;
  }
}
bool SocketPosixImpl::SetCallback(CallbackObj obj, IncomingSocketCallback cb) {
  if (!_mgr) {
    logw("Socket manager is NULL, pls check\n");
    return false;
  }
  _obj = obj;
  _incomingCb = cb;


  if (_mgr->AddSocket(this))
    return true;

  return false;
}
bool SocketPosixImpl::Bind(const socket_addr& name) {
  int size = sizeof(sockaddr);
  if (0 == bind(_socket, reinterpret_cast<const sockaddr*>(&name),size)) {
      return true;
  }
  loge("SocketPosix::Bind() error: %d", errno);
  return false;
}

bool SocketPosixImpl::StartReceiving() {
  if (!_mgr)
    return false;
  _wantsIncoming = true;
  return true;
}
bool SocketPosixImpl::StopReceiving() {
  _wantsIncoming = false;
  return true;
}
bool SocketPosixImpl::ValidHandle() {
  return _socket != INVALID_SOCKET;
}

bool SocketPosixImpl::SetSockopt(int32_t level, int32_t optname,
    const int8_t* optval, int32_t optlen) {
  if(0 == setsockopt(_socket, level, optname, optval, optlen )) {
    return true;
  }

  loge("SocketPosix::SetSockopt(), error:%d\n", errno);
  return false;
}
int32_t SocketPosixImpl::SetTOS(const int32_t serviceType) {
  if (SetSockopt(IPPROTO_IP, IP_TOS ,(int8_t*)&serviceType ,4) != 0) {
      return -1;
  }
  return 0;
}
int32_t SocketPosixImpl::SetPCP(const int32_t /*pcp*/){
  return -1;
}
int32_t SocketPosixImpl::SendTo(const int8_t* buf, int32_t len,
                         const socket_addr& to) {
  int size = sizeof(sockaddr);
  int retVal = sendto(_socket,buf, len, 0,
                      reinterpret_cast<const sockaddr*>(&to), size);
  if(retVal == SOCKET_ERROR) {
    loge("SocketPosix::SendTo() error: %d\n", errno);
  }

  logi("SocketPosix::SendTo() size: %d\n", retVal);
  return retVal;
}

bool SocketPosixImpl::SetQos(int32_t serviceType, int32_t tokenRate,
                      int32_t bucketSize, int32_t peekBandwith,
                      int32_t minPolicedSize, int32_t maxSduSize,
                      const socket_addr &stRemName,
                      int32_t overrideDSCP) {
  return false;
}
SOCKET SocketPosixImpl::GetFd() {
  return _socket;
}
bool SocketPosixImpl::CleanUp() {
  _wantsIncoming = false;

  if (_socket == INVALID_SOCKET) {
      return false;
  }

  loge("calling SocketManager::RemoveSocket()...\n");
  if (!_mgr) {
    logw("SocketManager is null\n");
    return false;
  }

  _mgr->RemoveSocket(this);
  return true;
}
void SocketPosixImpl::HasIncoming() {
  // replace 2048 with a mcro define and figure out
  // where 2048 comes from
  int8_t buf[2048];
  int retval;
  socket_addr from;
#if defined(_OS_MAC)
  sockaddr sockaddrfrom;
  memset(&from, 0, sizeof(from));
  memset(&sockaddrfrom, 0, sizeof(sockaddrfrom));
  socklen_t fromlen = sizeof(sockaddrfrom);
#else
  memset(&from, 0, sizeof(from));
  socklen_t fromlen = sizeof(from);
#endif

#if defined(_OS_MAC)
  retval = recvfrom(_socket,buf, sizeof(buf), 0,
      reinterpret_cast<sockaddr*>(&sockaddrfrom), &fromlen);
  memcpy(&from, &sockaddrfrom, fromlen);
  from._sockaddr_storage.sin_family = sockaddrfrom.sa_family;
#else
  retval = recvfrom(_socket,buf, sizeof(buf), 0,
      reinterpret_cast<sockaddr*>(&from), &fromlen);
#endif

  switch(retval)
  {
    case 0:
      // The peer has performed an orderly shutdown.
      break;
    case SOCKET_ERROR:
      break;
    default:
      if (_wantsIncoming && _incomingCb)
      {
        _incomingCb(_obj, buf, retval, &from);
      }
      break;
  }
}
bool SocketPosixImpl::WantsIncoming() {
  return _wantsIncoming;
}
void SocketPosixImpl::ReadyForDeletion() {
    _cs->lock();
    if(!_closeBlockingActive){
      _cs->unlock();
      return;
    }

    close(_socket);
    _socket = INVALID_SOCKET;
    _readyForDeletion = true;
    _readyForDeletionCond->set();
    if(!_closeBlockingCompleted)
    {
        _cs->unlock();
        _closeBlockingCompletedCond->wait(OS_EVENT_INFINITE);
        _cs->lock();
    }

}
void SocketPosixImpl::CloseBlocking()
{
  _cs->lock();
  _closeBlockingActive = true;
  if(!CleanUp()) {
    _closeBlockingActive = false;
    _cs->unlock();
    return;
  }

  if(!_readyForDeletion) {
    _cs->unlock();
    _readyForDeletionCond->wait(OS_EVENT_INFINITE);
    _cs->lock();
  }
  _closeBlockingCompleted = true;
  _closeBlockingCompletedCond->set();
}

} //namespace os
