#ifndef _OS_SOCKET_MANAGER_POSIX_H_
#define _OS_SOCKET_MANAGER_POSIX_H_

#include <list>
#include <map>
#include <sys/select.h>
#include <os_mutex.h>
#include <os_thread.h>
#include <os_socket.h>
#include <os_socket_posix.h>
#include <os_socket_manager.h>

namespace os {
#define MAX_NUMBER_OF_SOCKET_MANAGERS_LINUX 8

class SocketManagerPosixImpl;

class SocketManagerPosix : public SocketManager
{
  public:
    SocketManagerPosix();
    virtual ~SocketManagerPosix();

    bool Init(int32_t id, uint8_t& numOfWorkThreads) override;

    bool Start() override;
    bool Stop() override;

    uint8_t WorkThreads() const override;

    bool AddSocket(Socket* s) override;
    bool RemoveSocket(Socket* s) override;

  private:
    int32_t _id;
    Mutex*  _critSect;
    uint8_t _numberOfSocketMgr;
    uint8_t _numOfWorkThreads;
    uint8_t _incSocketMgrNextTime;
    uint8_t _nextSocketMgrToAssign;
    SocketManagerPosixImpl* _socketMgr[MAX_NUMBER_OF_SOCKET_MANAGERS_LINUX];
};

class SocketManagerPosixImpl
{
public:
  SocketManagerPosixImpl();
  virtual ~SocketManagerPosixImpl();

  virtual bool Start();
  virtual bool Stop();

  virtual bool AddSocket(Socket* s);
  virtual bool RemoveSocket(Socket* s);

protected:
  static bool thread_fun(void* obj);
  bool do_loop();
  void UpdateSocketMap();

private:
  typedef std::list<Socket*> SocketList;
  typedef std::list<SOCKET> FdList;
  Thread* _thread;
  Mutex*  _critSectList;

  fd_set _readFds;

  std::map<SOCKET, SocketPosixImpl*> _socketMap;
  SocketList _addList;
  FdList _removeList;
};

} //namespace os

#endif //_OS_SOCKET_MANAGER_POSIX_H_
