#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef _OS_LINUX
  #include <sys/types.h>
  #include <sched.h>
  #include <sys/syscall.h>
  #include <linux/unistd.h>
  #include <sys/prctl.h>
#endif

#include <os_typedefs.h>
#include <os_thread_posix.h>
#include <os_assert.h>

namespace os {

ThreadPosix::ThreadPosix(ThreadFun fun, void *ctx) :
                      _pid(-1),
                      _attr(),
                      _thread(-1),
                      _func(fun),
                      _obj(ctx),
                      _alive(false),
                      _dead(true),
                      _prio(eThreadPriorityNormal)
{
  int ret = 0;
#if !defined(_OS_ANDROID)
  // Enable immediate cancellation if requested, see Shutdown()
  ret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  CHECK_EQ(0, ret);
  ret = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  CHECK_EQ(0, ret);
#endif
  ret = pthread_attr_init(&_attr);
  CHECK_EQ(0, ret);
  _mutex = Mutex::Create();
  _cond = Cond::Create();
  strcpy(_name, "OSThread");
}

ThreadPosix::~ThreadPosix() {
  if (_cond)
    delete _cond;
  if (_mutex)
    delete _mutex;
  pthread_attr_destroy(&_attr);
}
void ThreadPosix::set_name(const char *name) {
  strncpy(_name, name, MIN(127, strlen(name)));
}
void ThreadPosix::set_priority(ThreadPriority priority) {
  _prio = priority;
}
int32_t ThreadPosix::get_pid() {
#if defined(_OS_ANDROID) || defined(_OS_LINUX)
  return static_cast<uint32_t>(syscall(__NR_gettid));
#elif defined(_OS_MAC)
  return static_cast<uint32_t>(mach_thread_self());
#else
  return reinterpret_cast<uint32_t>(pthread_self());
#endif
}
bool ThreadPosix::start(uint32_t & tid) {
  if (!_func)
    return false;

  int ret = pthread_attr_setdetachstate(&_attr, PTHREAD_CREATE_DETACHED);
  // Set the stack stack size to 1M.
  ret |= pthread_attr_setstacksize(&_attr, 1024*1024);
#ifdef WEBRTC_THREAD_RR
  const int policy = SCHED_RR;
#else
  const int policy = SCHED_FIFO;
#endif
  ret |= pthread_create(&_thread, &_attr, &thread_loop, this);
  if (ret)
    return false;

  _mutex->lock();
  _cond->wait(_mutex);
  _mutex->unlock();

  sched_param param;
  const int minPrio = sched_get_priority_min(policy);
  const int maxPrio = sched_get_priority_max(policy);
  if ((minPrio == EINVAL) || (maxPrio == EINVAL))
    return false;

  switch (_prio)
  {
  case eThreadPriorityLow:
      param.sched_priority = minPrio + 1;
      break;
  case eThreadPriorityNormal:
      param.sched_priority = (minPrio + maxPrio) / 2;
      break;
  case eThreadPriorityHigh:
      param.sched_priority = maxPrio - 3;
      break;
  case eThreadPriorityRealtime:
      param.sched_priority = maxPrio - 1;
      break;
  }
  ret = pthread_setschedparam(_thread, policy, &param);
  return ret == EINVAL ? false : true;
}
bool ThreadPosix::stop() {
  bool dead = false;
  _mutex->lock();
  _alive = false;
  dead = _dead;
  _mutex->unlock();

  for (int i = 0; i < 1000 && !dead; i++) {
    timespec t;
    t.tv_sec = 0;
    t.tv_nsec = 10*1000*1000;
    nanosleep(&t, NULL);
    _mutex->lock();
    dead = _dead;
    _mutex->unlock();
  }
  if (dead)
    return true;
  else
    return false;
}

void ThreadPosix::loop() {
  _mutex->lock();
  _alive = true;
  _dead = false;
  _mutex->unlock();

  _cond->signal();

  prctl(PR_SET_NAME, (unsigned long)_name, 0, 0, 0);

  bool alive = true;
  do {
    if (_func) {
      if (!_func(_obj))
        alive = false;
    } else {
      alive = false;
    }

    _mutex->lock();
    if (!alive) {
      _alive = false;
    }
    alive = _alive;
    _mutex->unlock();
  } while (alive);

  _mutex->lock();
  _dead = true;
  _mutex->unlock();
}

//static
void *ThreadPosix::thread_loop(void *obj) {
  ThreadPosix *pthis = static_cast<ThreadPosix*>(obj);
  pthis->loop();
  return NULL;
}

} //os
