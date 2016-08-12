#include <os_thread.h>

#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  #include <os_thread_posix.h>
#else
  #error "Unknown os"
#endif

namespace os {

//static
Thread *Thread::Create(ThreadFun fun, void *ctx) {
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  return new ThreadPosix(fun, ctx);
#else
  #error "Unknown os"
#endif
  return NULL;
}

} //namespace os
