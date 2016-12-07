#include <pthread.h>
#include <os_cond.h>

namespace os {

class CondPosix : public Cond {
private:
  pthread_cond_t _cond;
#ifndef _OS_CLOCK_REALTIME
  pthread_condattr_t _attr;
#endif
public:
  CondPosix();
  virtual ~CondPosix();
  virtual void wait(Mutex *mutex);
  virtual bool wait_timeout(Mutex *mutex, uint64_t ms);

  virtual void signal();
  virtual void broadcast();
};

} //namespace os
