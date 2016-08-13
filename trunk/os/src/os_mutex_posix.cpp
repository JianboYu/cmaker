#include <os_log.h>
#include <os_mutex_posix.h>

namespace os {

MutexPosix::MutexPosix() {
  pthread_mutexattr_t attr;
  (void) pthread_mutexattr_init(&attr);
  (void) pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  (void) pthread_mutex_init(&_mutex, &attr);
}

MutexPosix::~MutexPosix() {
  (void) pthread_mutex_destroy(&_mutex);
}

void MutexPosix::lock() {
  (void) pthread_mutex_lock(&_mutex);
}

void MutexPosix::unlock() {
  (void) pthread_mutex_unlock(&_mutex);
}

} //namespace os
