#include <os_typedefs.h>
#include <os_mutex.h>
#include <os_assert.h>
#include <os_thread.h>
#include <os_time.h>
#include <os_isocket.h>
#include <core_scoped_ptr.h>

using namespace os;
using namespace core;

int32_t main(int32_t argc, char *argv[]) {
  IPAddress ipaddr(INADDR_ANY);
  SocketAddress socket_addr(ipaddr, 8080);
  //socket_addr.SetIP("172.16.1.88");
  //socket_addr.SetPort(8000);

  log_verbose("tag", "socket addr: %s \n", socket_addr.to_string().c_str());

  ISocket *socket = ISocket::Create(AF_INET, SOCK_STREAM/*SOCK_DGRAM*/, IPPROTO_TCP);
  //SocketAddress socket_addr2 = socket->GetLocalAddress();
  //log_verbose("tag", "socket addr: %s \n", socket_addr2.to_string().c_str());

  CHECK_EQ(0, socket->bind(socket_addr));
  CHECK_EQ(0, socket->listen(5));
  SocketAddress client_socket_addr;
  ISocket *client = socket->accept(&client_socket_addr);
  CHECK(client);
  log_verbose("tag", "client socket addr: %s \n", client_socket_addr.to_string().c_str());

  uint8_t buf[1024] = {0};
  int64_t timestamp = 0;
  while(1) {
    client->recv(buf, 1024, &timestamp);
    logv("[Server]---ts: %lld str: %s\n", timestamp, buf);
    client->send("Got it\n", 7);
    os_msleep(1000);
  }
}
