#include <os_log.h>
#include <os_mutex_posix.h>

namespace os {

MutexPosix::MutexPosix() {
  (void) pthread_mutexattr_init(&_attr);
  (void) pthread_mutexattr_settype(&_attr, PTHREAD_MUTEX_RECURSIVE);
  (void) pthread_mutex_init(&_mutex, &_attr);
}

MutexPosix::~MutexPosix() {
  (void) pthread_mutexattr_destroy(&_attr);
  (void) pthread_mutex_destroy(&_mutex);
}

void MutexPosix::lock() {
  (void) pthread_mutex_lock(&_mutex);
}

void MutexPosix::unlock() {
  (void) pthread_mutex_unlock(&_mutex);
}

} //namespace os
