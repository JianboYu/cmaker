#include <os_cond.h>

#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  #include <os_cond_posix.h>
#else
  #error "Unknown os"
#endif

namespace os {

//static
Cond * Cond::Create() {
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  return new CondPosix();
#else
  #error "Unknown os"
#endif
  return NULL;
}

} //namespace os
