#include <os_assert.h>
#include "os_event_posix.h"

namespace os {

EventPosix::EventPosix()
    : _timer_thread(0),
      _timer_event(0),
      _periodic(false),
      _time(0),
      _count(0),
      _state(kDown) {
  _cond = Cond::Create();
  _mutex = Mutex::Create();
}

EventPosix::~EventPosix() {
  stop_timer();

  if (_mutex) {
    delete _mutex;
    _mutex = NULL;
  }
  if (_cond) {
    delete _cond;
    _cond = NULL;
  }
}

bool EventPosix::set() {
  _mutex->lock();
  _state = kUp;
  _mutex->unlock();
  _cond->broadcast();
  return true;
}
bool EventPosix::reset() {
  _mutex->lock();
  _state = kDown;
  _mutex->unlock();
  return true;
}
EventType EventPosix::wait(uint64_t timeout_ms) {
  bool ret = false;
  _mutex->lock();

  if (kDown == _state)
  {
    if (OS_EVENT_INFINITE != timeout_ms)
    {
      ret = _cond->wait_timeout(_mutex, timeout_ms);
    } else {
      _cond->wait(_mutex);
      ret = true;
    }
  }

  _state = kDown;
  _mutex->unlock();

  switch(ret)
  {
    case true:
      return kEventSignaled;
    default:
      return kEventTimeout;
      //return kEventError;
  }
}
bool EventPosix::start_timer(bool periodic, uint64_t time_ms) {
  if (_timer_thread)
  {
    if(_periodic)
    {
      // Timer already started.
      return false;
    } else  {
      // New one shot timer
      _time = time_ms;
      _timer_event->set();
      return true;
    }
  }

  // Start the timer thread
  _timer_event = new EventPosix();
  _timer_thread = Thread::Create(timer_thread_fun, this);
  _timer_thread->set_name("os_event_timer_thread");

  _periodic = periodic;
  _time = time_ms;
  uint32_t tid = 0;
  if (_timer_thread->start(tid))
  {
    return true;
  }
  return false;
}
bool EventPosix::stop_timer() {
  if (_timer_event) {
    _timer_event->set();
    delete _timer_event;
    _timer_event = NULL;
  }
  if (_timer_thread) {
    _timer_thread->stop();
    delete _timer_thread;
    _timer_thread = NULL;
  }
  _count=0;
  return true;
}
bool EventPosix::timer_thread_fun(void * obj) {
  return static_cast<EventPosix*>(obj)->do_loop();
}
bool EventPosix::do_loop() {
  ++_count;
  switch(_timer_event->wait(_time))
  {
    case kEventSignaled:
      return true;
    case kEventError:
      return false;
    case kEventTimeout:
      break;
  }
  if(_periodic || _count==1)
  {
    set();
  }
  return true;
}

} //namespace os
