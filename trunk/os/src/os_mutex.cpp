#include <os_mutex.h>

#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  #include <os_mutex_posix.h>
#else
  #error "Unknown os"
#endif

namespace os {

//static
Mutex *Mutex::Create() {
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  return new MutexPosix();
#else
  #error "Unknown os"
#endif
  return NULL;
}

} //namespace os
