#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <os_typedefs.h>
#include <os_mutex.h>
#include <os_assert.h>
#include <os_thread.h>
#include <os_time.h>
#include <os_socket.h>
#include <os_socket_manager.h>

#include <os_socket.h>
#include <os_isocket.h>
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

static const int64_t kNumMillisecsPerSec = INT64_C(1000);
static const int64_t kNumMicrosecsPerSec = INT64_C(1000000);
static const int64_t kNumNanosecsPerSec = INT64_C(1000000000);

static void IncomingSocketCallbackFun(CallbackObj obj, const int8_t* buf,
                                      int32_t len, const socket_addr* from) {
  logi("Recieving buf: %p size: %d \n", buf, len);
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

  IPAddress ip_addr(123456);
  log_verbose("tag", "ip: %s\n", ip_addr.to_string().c_str());

  SocketAddress socket_addr_v(ip_addr, 50);
  socket_addr_v.set_ip("127.0.0.1");
  socket_addr_v.set_port(8008);
  log_verbose("tag", "socket addr: %s \n", socket_addr_v.to_string().c_str());

  scoped_ptr<ISocket> isocket(ISocket::Create(AF_INET, SOCK_STREAM/*SOCK_DGRAM*/, IPPROTO_TCP));
  SocketAddress socket_addr2 = isocket->local_address();
  log_verbose("tag", "socket addr: %s \n", socket_addr2.to_string().c_str());

  CHECK_EQ(0, isocket->bind(socket_addr_v));
  CHECK_EQ(0, isocket->listen(5));
  SocketAddress client_socket_addr;
  ISocket *client = isocket->accept(&client_socket_addr);
  CHECK(client);
  log_verbose("tag", "client socket addr: %s \n", client_socket_addr.to_string().c_str());

  socket_addr socket_addr_src;
  socket_addr_src._sockaddr_in.sin_family = AF_INET;
  socket_addr_src._sockaddr_in.sin_port = htons(50000);
  socket_addr_src._sockaddr_in.sin_addr = inet_addr("172.16.1.88");

  socket_addr socket_addr_des;
  socket_addr_des._sockaddr_in.sin_family = AF_INET;
  socket_addr_des._sockaddr_in.sin_port = htons(50050);
  socket_addr_des._sockaddr_in.sin_addr = inet_addr("172.16.104.13");
  uint8_t numThreads = 1;
  scoped_ptr<SocketManager> socket_manager(SocketManager::Create(0, numThreads));
  scoped_ptr<Socket> socket(Socket::CreateSocket(0, socket_manager.get(),
                                                NULL, IncomingSocketCallbackFun, false, false));
  CHECK_EQ(1, socket->StartReceiving());
  CHECK_EQ(1, socket->Bind(socket_addr_src));
  int8_t buf[10] = {'a', 'b', 'c'};
  CHECK_EQ(10, socket->SendTo(buf, 10, socket_addr_des));

  while(1) {
    os_msleep(1000);
    mutex->lock();
    os_msleep(100);
    mutex->unlock();
  }
}
