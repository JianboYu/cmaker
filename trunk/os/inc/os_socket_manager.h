#ifndef _OS_SOCKET_MANAGER_H_
#define _OS_SOCKET_MANAGER_H_

#include <os_typedefs.h>
namespace os {

class Socket;

class SocketManager
{
public:
  static SocketManager* Create(const int32_t id,
      uint8_t& numOfWorkThreads);

  virtual ~SocketManager() {}
  // Initializes the socket manager. Returns true if the manager wasn't
  // already initialized.
  virtual bool Init(int32_t id, uint8_t& numOfWorkThreads) = 0;

  // Start listening to sockets that have been registered via the
  // AddSocket(..) API.
  virtual bool Start() = 0;
  // Stop listening to sockets.
  virtual bool Stop() = 0;

  virtual uint8_t WorkThreads() const = 0;

  // Register a socket with the socket manager.
  virtual bool AddSocket(Socket* s) = 0;
  // Unregister a socket from the manager.
  virtual bool RemoveSocket(Socket* s) = 0;

protected:
  SocketManager() {}
};

} //namespace os

#endif //_OS_SOCKET_MANAGER_H_
