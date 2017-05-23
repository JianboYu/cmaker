#include <errno.h>
#include <time.h>

#include <os_typedefs.h>
#include <os_cond_posix.h>
#include <os_mutex_posix.h>
#include <os_assert.h>

namespace os {
CondPosix::CondPosix() {
  int32_t ret = 0;
#ifdef _OS_CLOCK_REALTIME
  ret = pthread_cond_init(&_cond, NULL);
  CHECK_EQ(0, ret);
#else
  ret = pthread_condattr_init(&_attr);
  CHECK_EQ(0, ret);
  ret = pthread_condattr_setclock(&_attr, CLOCK_MONOTONIC);
  CHECK_EQ(0, ret);
  ret = pthread_cond_init(&_cond, &_attr);
  CHECK_EQ(0, ret);
#endif
}
CondPosix::~CondPosix() {
#ifndef _OS_CLOCK_REALTIME
  pthread_condattr_destroy(&_attr);
#endif

  pthread_cond_destroy(&_cond);
}
void CondPosix::wait(Mutex *mut) {
  MutexPosix* mutex = reinterpret_cast<MutexPosix*>(mut);
  pthread_cond_wait(&_cond, &mutex->_mutex);
}
bool CondPosix::wait_timeout(Mutex *mut, uint64_t ms) {
  timespec ts;
#ifdef _OS_CLOCK_REALTIME
  clock_gettime(CLOCK_REALTIME, &ts);
#else
  clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
  ts.tv_nsec += (ms - ((ms / 1000)* 1000)) * 1000000;

  if (ts.tv_nsec >= 1000000000)
  {
    ts.tv_sec += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;
  }
  MutexPosix* mutex = reinterpret_cast<MutexPosix*>(mut);
  int32_t ret = pthread_cond_timedwait(&_cond, &mutex->_mutex, &ts);
  return ret == ETIMEDOUT ? false : true;
}
void CondPosix::signal() {
  pthread_cond_signal(&_cond);
}
void CondPosix::broadcast() {
  pthread_cond_broadcast(&_cond);
}

} //namespace os
