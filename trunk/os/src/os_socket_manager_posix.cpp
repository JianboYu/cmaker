#include <os_assert.h>
#include <os_socket_manager_posix.h>
#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG "SocketManagerPosix"
#include <os_log.h>
#include <os_time.h>

namespace os {
SocketManagerPosix::SocketManagerPosix()
  : _id(-1),
  _critSect(Mutex::Create()),
  _numberOfSocketMgr(-1),
  _incSocketMgrNextTime(0),
  _nextSocketMgrToAssign(0),
  _socketMgr()
{
}

bool SocketManagerPosix::Init(int32_t id, uint8_t& numOfWorkThreads) {
  _critSect->lock();
  if ((_id != -1) || (_numOfWorkThreads != 0)) {
    CHECK(_id != -1);
    CHECK(_numOfWorkThreads != 0);
    _critSect->unlock();
    return false;
  }

  _id = id;
  _numberOfSocketMgr = numOfWorkThreads;
  _numOfWorkThreads = numOfWorkThreads;

  if(MAX_NUMBER_OF_SOCKET_MANAGERS_LINUX < _numberOfSocketMgr)
  {
    _numberOfSocketMgr = MAX_NUMBER_OF_SOCKET_MANAGERS_LINUX;
  }
  for(int i = 0;i < _numberOfSocketMgr; i++)
  {
    _socketMgr[i] = new SocketManagerPosixImpl();
  }
  return true;
}

SocketManagerPosix::~SocketManagerPosix()
{
  Stop();
  logi("SocketManagerPosix(%d)::SocketManagerPosix()\n", _numberOfSocketMgr);

  for(int i = 0;i < _numberOfSocketMgr; i++) {
    delete _socketMgr[i];
  }
  delete _critSect;
}

bool SocketManagerPosix::Start()
{
  logi("SocketManagerPosix(%d)::Start()",_numberOfSocketMgr);

  _critSect->lock();
  bool retVal = true;
  for(int i = 0;i < _numberOfSocketMgr && retVal; i++) {
    retVal = _socketMgr[i]->Start();
  }
  if(!retVal) {
    loge("SocketManagerPosix(%d)::Start() error starting socket managers\n",
        _numberOfSocketMgr);
  }
  _critSect->unlock();
  return retVal;
}

bool SocketManagerPosix::Stop()
{
  logi("SocketManagerPosix(%d)::Stop()\n", _numberOfSocketMgr);

  _critSect->lock();
  bool retVal = true;
  for(int i = 0; i < _numberOfSocketMgr && retVal; i++) {
    retVal = _socketMgr[i]->Stop();
  }
  if(!retVal) {
    loge("SocketManagerPosix(%d)::Stop() there are still active socket "
        "managers",
        _numberOfSocketMgr);
  }
  _critSect->unlock();
  return retVal;
}

uint8_t SocketManagerPosix::WorkThreads() const {
  return _numOfWorkThreads;
}

bool SocketManagerPosix::AddSocket(Socket* s)
{
  logi("SocketManagerPosix(%d)::AddSocket()\n",_numberOfSocketMgr);

  _critSect->lock();
  bool retVal = _socketMgr[_nextSocketMgrToAssign]->AddSocket(s);
  if(!retVal) {
    loge("SocketManagerPosix(%d)::AddSocket() failed to add socket to\
        manager\n", _numberOfSocketMgr);
  }

  // Distribute sockets on SocketManagerPosixImpls in a round-robin
  // fashion.
  if(_incSocketMgrNextTime == 0)
  {
    _incSocketMgrNextTime++;
  } else {
    _incSocketMgrNextTime = 0;
    _nextSocketMgrToAssign++;
    if(_nextSocketMgrToAssign >= _numberOfSocketMgr)
    {
      _nextSocketMgrToAssign = 0;
    }
  }
  _critSect->unlock();
  return retVal;
}

bool SocketManagerPosix::RemoveSocket(Socket* s) {
  loge("SocketManagerPosix(%d)::RemoveSocket()\n",
      _numberOfSocketMgr);

  _critSect->lock();
  bool retVal = false;
  for(int i = 0;i < _numberOfSocketMgr && (retVal == false); i++) {
    retVal = _socketMgr[i]->RemoveSocket(s);
  }
  if(!retVal) {
    loge("SocketManagerPosix(%d)::RemoveSocket() failed to remove socket\
        from manager", _numberOfSocketMgr);
  }
  _critSect->unlock();
  return retVal;
}

SocketManagerPosixImpl::SocketManagerPosixImpl()
  :_critSectList(Mutex::Create()) {
    FD_ZERO(&_readFds);
    _thread = Thread::Create(thread_fun, this);
    _thread->set_name("SocketManagerPosixImplThread");
  }

SocketManagerPosixImpl::~SocketManagerPosixImpl()
{
  if (_critSectList != NULL)
  {
    UpdateSocketMap();

    _critSectList->lock();
    for (std::map<SOCKET, SocketPosixImpl*>::iterator it =
        _socketMap.begin();
        it != _socketMap.end();
        ++it) {
      delete it->second;
    }
    _socketMap.clear();
    _critSectList->unlock();

    delete _critSectList;
  }

  logi("SocketManagerPosix deleted\n");
}

bool SocketManagerPosixImpl::Start()
{
  logi("Start SocketManagerPosix\n");
  uint32_t tid;
  _thread->start(tid);
  return true;
}

bool SocketManagerPosixImpl::Stop()
{
  logi("Stop SocketManagerPosix");
  _thread->stop();
  return true;
}

bool SocketManagerPosixImpl::do_loop()
{
  bool doSelect = false;
  // Timeout = 1 second.
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 10000;

  FD_ZERO(&_readFds);

  UpdateSocketMap();

  SOCKET maxFd = 0;
  for (std::map<SOCKET, SocketPosixImpl*>::iterator it = _socketMap.begin();
      it != _socketMap.end();
      ++it) {
    doSelect = true;
    if (it->first > maxFd)
      maxFd = it->first;
    FD_SET(it->first, &_readFds);
  }

  int num = 0;
  if (doSelect)
  {
    num = select(maxFd+1, &_readFds, NULL, NULL, &timeout);

    if (num == SOCKET_ERROR)
    {
      // Timeout = 10 ms.
      os_msleep(10);
      return true;
    }
  }else
  {
    // Timeout = 10 ms.
    os_msleep(10);
    return true;
  }
  std::map<SOCKET, SocketPosixImpl*>::iterator it;
  for (it = _socketMap.begin();
      it != _socketMap.end();
      ++it) {
    if (FD_ISSET(it->first, &_readFds)) {
      it->second->HasIncoming();
      --num;
    }
  }

  return true;
}

bool SocketManagerPosixImpl::thread_fun(void* obj)
{
  SocketManagerPosixImpl* mgr =
    static_cast<SocketManagerPosixImpl*>(obj);
  return mgr->do_loop();
}

bool SocketManagerPosixImpl::AddSocket(Socket* s)
{
  logi("AddSocket : %p\n", s);
  SocketPosixImpl* sl = static_cast<SocketPosixImpl*>(s);
  if(sl->GetFd() == INVALID_SOCKET || !(sl->GetFd() < FD_SETSIZE))
  {
    return false;
  }
  _critSectList->lock();
  _addList.push_back(s);
  _critSectList->unlock();
  return true;
}

bool SocketManagerPosixImpl::RemoveSocket(Socket* s)
{
  logi("RemoveSocket : %p\n", s);
  // Put in remove list if this is the correct SocketManagerPosixImpl.
  _critSectList->lock();

  // If the socket is in the add list it's safe to remove and delete it.
  for (SocketList::iterator iter = _addList.begin();
      iter != _addList.end(); ++iter) {
    SocketPosixImpl* addSocket = static_cast<SocketPosixImpl*>(*iter);
    unsigned int addFD = addSocket->GetFd();
    unsigned int removeFD = static_cast<SocketPosixImpl*>(s)->GetFd();
    if(removeFD == addFD)
    {
      _removeList.push_back(removeFD);
      _critSectList->unlock();
      return true;
    }
  }

  // Checking the socket map is safe since all Erase and Insert calls to this
  // map are also protected by _critSectList.
  if (_socketMap.find(static_cast<SocketPosixImpl*>(s)->GetFd()) !=
      _socketMap.end()) {
    _removeList.push_back(static_cast<SocketPosixImpl*>(s)->GetFd());
    _critSectList->unlock();
    return true;
  }
  _critSectList->unlock();
  return false;
}

void SocketManagerPosixImpl::UpdateSocketMap()
{
  // Remove items in remove list.
  _critSectList->lock();
  for (FdList::iterator iter = _removeList.begin();
      iter != _removeList.end(); ++iter) {
    SocketPosixImpl* deleteSocket = NULL;
    SOCKET removeFD = *iter;

    // If the socket is in the add list it hasn't been added to the socket
    // map yet. Just remove the socket from the add list.
    for (SocketList::iterator iter = _addList.begin();
        iter != _addList.end(); ++iter) {
      SocketPosixImpl* addSocket = static_cast<SocketPosixImpl*>(*iter);
      SOCKET addFD = addSocket->GetFd();
      if(removeFD == addFD)
      {
        deleteSocket = addSocket;
        _addList.erase(iter);
        break;
      }
    }

    // Find and remove socket from _socketMap.
    std::map<SOCKET, SocketPosixImpl*>::iterator it =
      _socketMap.find(removeFD);
    if(it != _socketMap.end())
    {
      deleteSocket = it->second;
      _socketMap.erase(it);
    }
    if(deleteSocket)
    {
      deleteSocket->ReadyForDeletion();
      delete deleteSocket;
    }
  }
  _removeList.clear();

  // Add sockets from add list.
  for (SocketList::iterator iter = _addList.begin();
      iter != _addList.end(); ++iter) {
    SocketPosixImpl* s = static_cast<SocketPosixImpl*>(*iter);
    if(s) {
      _socketMap[s->GetFd()] = s;
    }
  }
  _addList.clear();
  _critSectList->unlock();
}

} //namespace os
