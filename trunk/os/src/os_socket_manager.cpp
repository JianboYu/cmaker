#include <os_socket_manager.h>
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  #include <os_socket_manager_posix.h>
#else
  #error "Unknown os"
#endif

namespace os {

SocketManager* SocketManager::Create(const int32_t id,
                      uint8_t& numOfWorkThreads) {
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  SocketManagerPosix *pIns = new SocketManagerPosix();
  if (pIns) {
    pIns->Init(id, numOfWorkThreads);
    pIns->Start();
    return pIns;
  }
#else
  #error "Unknown os"
#endif
  return NULL;
}

} //namespace os
