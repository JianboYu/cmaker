#include <os_typedefs.h>
#include <os_mutex.h>
#include <os_assert.h>
#include <os_thread.h>
#include <os_time.h>
#include <os_socket.h>
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
  log_verbose("tag", "ip: %s\n", ip_addr.ToString().c_str());

  SocketAddress socket_addr(ip_addr, 50);
  socket_addr.SetIP("127.0.0.1");
  socket_addr.SetPort(8008);
  log_verbose("tag", "socket addr: %s \n", socket_addr.ToString().c_str());

  scoped_ptr<Socket> socket(Socket::Create(AF_INET, SOCK_STREAM/*SOCK_DGRAM*/, IPPROTO_TCP));
  //SocketAddress socket_addr2 = socket->GetLocalAddress();
  //log_verbose("tag", "socket addr: %s \n", socket_addr2.ToString().c_str());

  CHECK_EQ(0, socket->Bind(socket_addr));
  CHECK_EQ(0, socket->Listen(5));
  SocketAddress client_socket_addr;
  Socket *client = socket->Accept(&client_socket_addr);
  CHECK(client);
  log_verbose("tag", "client socket addr: %s \n", client_socket_addr.ToString().c_str());
  while(1) {
    os_msleep(1000);
    mutex->lock();
    os_msleep(100);
    mutex->unlock();
  }
}
