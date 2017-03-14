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
  scoped_ptr<ISocket> socket_client(ISocket::Create(AF_INET, SOCK_STREAM, IPPROTO_TCP));
  IPAddress ipaddr(0);
  SocketAddress socket_addr(ipaddr, 50);
  socket_addr.SetIP("127.0.0.1");
  socket_addr.SetPort(8080);

  log_verbose("tag", "connect socket addr: %s \n", socket_addr.ToString().c_str());
  CHECK_EQ(0, socket_client->Connect(socket_addr));

  const char *contend = "Hello i am client\n";
  uint8_t buf[1024] = {0};
  int64_t timestamp = 0;
  while(1) {
    socket_client->Send(contend, strlen(contend));
    socket_client->Recv(buf, 1024, &timestamp);
    logv("[Client]---ts: %lld str: %s\n", timestamp, buf);
    os_msleep(1000);
  }
  return 0;
}
