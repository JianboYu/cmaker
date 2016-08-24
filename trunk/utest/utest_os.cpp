#include <os_typedefs.h>
#include <os_mutex.h>
#include <os_assert.h>
#include <os_thread.h>
#include <os_time.h>
#include <core_scoped_ptr.h>

using namespace os;
using namespace core;

bool loop(void *ctx) {
  os_msleep(1000);
  Mutex *mutex = static_cast<Mutex*>(ctx);
  mutex->lock();
  mutex->lock();
  os_msleep(100);
  mutex->unlock();
  mutex->unlock();
  log_verbose("tag" , "systime: %lld\n", os_get_monitictime());
  return true;
}

int32_t main(int32_t argc, char *argv[]) {
  int a __attribute__((__unused__)) = 5;
  {
    scoped_ptr<Mutex> mutex_scope(Mutex::Create());
  }
  scoped_ptr<Mutex> mutex(Mutex::Create());

  CHECK(mutex.get());
  CHECK_NE((void*)NULL, mutex.get());
  log_verbose("tag", "test log int(%d) char(%c) string(%s)\n", 0x12, 0x48, "verborse log");
  log_error("tag", "test log int(%d) char(%c) string(%s)\n", 0x12, 0x48, "verborse log");

  Thread *pthread = Thread::Create(loop, mutex.get());
  uint32_t tid = 0;
  pthread->start(tid);
  log_verbose("tag", "created thread tid: %d\n", tid);

  while(1) {
    os_msleep(1000);
    mutex->lock();
    os_msleep(100);
    mutex->unlock();
  }
}
