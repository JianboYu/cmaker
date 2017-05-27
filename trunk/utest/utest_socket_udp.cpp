#include <arpa/inet.h>
#include <os_typedefs.h>
#include <os_assert.h>
#include <core_scoped_ptr.h>
#include <os_time.h>
#include <os_socket.h>
#include <os_socket_manager.h>

using namespace os;
using namespace core;

static void incomingSocketCallback(CallbackObj obj, const int8_t* buf,
                                      int32_t len, const socket_addr* from) {
  logv("incoming buf[%p] size[%d] from[%p]\n", buf, len, from);
  logv("content: %s\n", buf);
}

int32_t main(int32_t argc, char *argv[]) {
  uint8_t threads = 1;
  SocketManager *sock_mgr = SocketManager::Create(0, threads);
  CHECK(sock_mgr);
  Socket *sock = Socket::CreateSocket(0, sock_mgr,
      NULL,
      incomingSocketCallback);
  CHECK(sock);
  socket_addr sock_addr;
  sock_addr._sockaddr_in.sin_family = AF_INET;
  sock_addr._sockaddr_in.sin_port = htons(15050);
  sock_addr._sockaddr_in.sin_addr = inet_addr("172.16.1.88");

  CHECK_EQ(true, sock->Bind(sock_addr));
  CHECK_EQ(true, sock->StartReceiving());

  socket_addr sock_dst_addr;
  sock_dst_addr._sockaddr_in.sin_family = AF_INET;
  sock_dst_addr._sockaddr_in.sin_port = htons(25050);
  sock_dst_addr._sockaddr_in.sin_addr = inet_addr("172.16.104.13");

  int8_t szBuf[30] = {"Hello"};
  while (1) {
    int32_t ret = sock->SendTo(szBuf, 30, sock_dst_addr);
    logv("SendTo ret: %d\n", ret);
    os_sleep(1);
  }

  getchar();

  delete sock_mgr;
  delete sock;

  return 0;
}
