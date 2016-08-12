#include <os_typedefs.h>
#include <os_mutex.h>

using namespace os;
int32_t main(int32_t argc, char *argv[]) {
  int a __attribute__((__unused__)) = 5;
  Mutex *mutex = Mutex::Create();
  delete mutex;
}
