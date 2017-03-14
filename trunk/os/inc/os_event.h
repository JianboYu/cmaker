#ifndef _OS_EVENT_H_
#define _OS_EVENT_H_

#include <os_typedefs.h>

namespace os {

enum EventType
{
    kEventSignaled = 1,
    kEventError = 2,
    kEventTimeout = 3
};

#define OS_EVENT_10_SEC   (10000)
#define OS_EVENT_INFINITE (0xffffffff)

class Event
{
public:
  static Event* Create();
  virtual ~Event() {}

  virtual bool set() = 0;
  virtual bool reset() = 0;
  virtual EventType wait(uint64_t max_time_ms) = 0;
  virtual bool start_timer(bool periodic, uint64_t time_ms) = 0;
  virtual bool stop_timer() = 0;
};

} //namespace os

#endif //_OS_EVENT_H_
