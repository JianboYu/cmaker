#include <os_socket.h>
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  #include <os_socket_posix.h>
#else
  #error "Unknown os"
#endif

namespace os {
Socket * Socket::CreateSocket(const int32_t id,
                                SocketManager* mgr,
                                CallbackObj obj,
                                IncomingSocketCallback cb,
                                bool ipV6Enable,
                                bool disableGQOS) {
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  SocketPosixImpl *pIns = new SocketPosixImpl(id, mgr,
                             ipV6Enable,
                             disableGQOS);
  if (pIns) {
    pIns->SetCallback(obj, cb);
    return pIns;
  }
#else
  #error "Unknown os"
#endif
  return NULL;
}

} //namespace os
