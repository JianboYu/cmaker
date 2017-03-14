#include "os_event.h"

#if defined(_OS_WINDOWS)
  #include <windows.h>
  #include "os_event_win.h"
#else
  #include <pthread.h>
  #include "os_event_posix.h"
#endif

namespace os {
Event * Event::Create()
{
#if defined(_OS_WINDOWS)
  return new EventWindows();
#else
  return new EventPosix();
#endif
}
}//namespace os

