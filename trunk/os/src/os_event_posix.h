#ifndef _OS_EVENT_POSIX_H_
#define _OS_EVENT_POSIX_H_

#include <os_cond.h>
#include <os_mutex.h>
#include <os_thread.h>
#include "os_event.h"

namespace os {
enum State
{
  kUp = 1,
  kDown = 2
};

class EventPosix : public Event {
private:
  Cond *_cond;
  Mutex *_mutex;

  Thread*     _timer_thread;
  EventPosix* _timer_event;

  bool     _periodic;
  uint64_t _time;  // In ms
  uint64_t _count;
  State    _state;

  static bool timer_thread_fun(void * obj);
  bool do_loop();

public:
  EventPosix();
  ~EventPosix();

  virtual bool set();
  virtual bool reset();
  virtual EventType wait(uint64_t timeout_ms);
  virtual bool start_timer(bool periodic, uint64_t time_ms);
  virtual bool stop_timer();
};

} //namespace os

#endif //_OS_EVENT_POSIX_H_
