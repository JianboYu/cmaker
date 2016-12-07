#include <pthread.h>
#include <os_mutex.h>

namespace os {
class MutexPosix : public Mutex {
private:
  pthread_mutexattr_t _attr;
  pthread_mutex_t _mutex;
  friend class CondPosix;
public:
  MutexPosix();
  ~MutexPosix();

  virtual void lock();
  virtual void unlock();
};

} //namespace os
