#ifndef _OS_THREAD_H_
#define _OS_THREAD_H_
#include <os_typedefs.h>

namespace os {

typedef bool (*ThreadFun)(void *ctx);

enum ThreadPriority {
  eThreadPriorityLow = 0,
  eThreadPriorityNormal = 1,
  eThreadPriorityHigh = 2,
  eThreadPriorityRealtime = 99
};

class Thread {
public:
  static Thread *Create(ThreadFun fun, void *ctx);
  virtual ~Thread() {}

  virtual void set_name(const char *name) = 0;
  virtual void set_priority(ThreadPriority priority) = 0;

  virtual int32_t get_pid() = 0;

  virtual bool start(uint32_t & tid) = 0;
  virtual bool stop() = 0;
};

} //namespace os

#endif //_OS_THREAD_H_


