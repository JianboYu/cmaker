#ifndef _OS_COND_H_
#define _OS_COND_H_

#include <os_typedefs.h>
#include <os_mutex.h>

namespace os {
class Cond {
public:
  static Cond * Create();
  virtual ~Cond() {}

  virtual void wait(Mutex *mutex) = 0;
  virtual bool wait_timeout(Mutex *mutex, uint64_t ms) = 0;

  virtual void signal() = 0;
  virtual void broadcast() = 0;
};

} //namespace os

#endif //_OS_COND_H_
