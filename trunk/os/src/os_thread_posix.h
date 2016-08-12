#include <pthread.h>
#include <os_thread.h>
#include <os_mutex.h>
#include <os_cond.h>

namespace os {

class ThreadPosix : public Thread {
private:
  pid_t          _pid;
  pthread_attr_t _attr;
  pthread_t      _thread;

  char _name[64];

  ThreadFun     _func;
  void *        _obj;

  Mutex             *_mutex;
  Cond              *_cond;
  bool              _alive;
  bool              _dead;
  ThreadPriority    _prio;

  void loop();
  static void *thread_loop(void *obj);
public:
  ThreadPosix(ThreadFun fun, void *ctx);
  ~ThreadPosix();

  virtual void set_name(const char *name);
  virtual void set_priority(ThreadPriority priority);

  virtual int32_t get_pid();

  virtual bool start(uint32_t & tid);
  virtual bool stop();

};

} //namespace os
