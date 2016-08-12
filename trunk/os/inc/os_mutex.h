#ifndef _OS_MUTEX_H_
#define _OS_MUTEX_H_

namespace os {

class Mutex {
public:
  static Mutex *Create();

  virtual ~Mutex() {};

  virtual void lock() = 0;
  virtual void unlock() = 0;
};

class AutoLock {
private:
  Mutex *_mutex;
public:
  explicit AutoLock(Mutex & mutex): _mutex(&mutex) {
    _mutex->lock();
  }
  explicit AutoLock(Mutex * mutex): _mutex(mutex) {
    _mutex->lock();
  }

  ~AutoLock() {
    _mutex->unlock();
  }
};

} //namespace os

#endif //_OS_MUTEX_H_
