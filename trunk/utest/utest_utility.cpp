#include <string.h>
#include <os_typedefs.h>
#include <os_mutex.h>
#include <os_assert.h>
#include <os_thread.h>
#include <os_time.h>
#include <core_scoped_ptr.h>
#include <utility_buffer_queue.h>
#include <utility_memory.h>

using namespace os;
using namespace core;
using namespace utility;

bool wr_loop(void *ctx) {
  static int32_t s_times = 0;
  ++s_times;
  buffer_queue_handle bq = static_cast<buffer_queue_handle>(ctx);
  int32_t array_idx = -1;
  if (0 != buffer_queue_get_empty_timeout(bq, &array_idx, 0)){
    return true;
  }

  int32_t array_idx2 = -1;
  if (s_times % 8 == 0) {
    buffer_queue_get_empty_timeout(bq, &array_idx2, BQ_TIMEOUT_INFINIT);
    buffer_queue_put_full(bq, array_idx2);
    log_verbose("tag" , "put wr id: %d\n", array_idx2);
  }
  os_msleep(40);
  log_verbose("tag" , "wr id: %d\n", array_idx);
  buffer_queue_put_full(bq, array_idx);
  return true;
}

bool rd_loop(void *ctx) {
  buffer_queue_handle bq = static_cast<buffer_queue_handle>(ctx);
  int32_t array_idx = -1;
  if (0 != buffer_queue_get_full_timeout(bq, &array_idx, 0)){
    return true;
  }
  os_msleep(40);
  log_verbose("tag" , "\t\trd id: %d\n", array_idx);
  buffer_queue_put_empty(bq, array_idx);
  return true;
}


int32_t bq_main(int32_t argc, char *argv[]) {
  buffer_queue_handle bq;
  void *array_ptr[5] = {(void*)0x11,(void*)0x12,(void*)0x13,(void*)0x14,(void*)0x15};
  CHECK_EQ(0, buffer_queue_create(&bq, array_ptr, 5));

  scoped_ptr<Thread> wr_thread(Thread::Create(rd_loop, bq));
  scoped_ptr<Thread> rd_thread(Thread::Create(wr_loop, bq));
  uint32_t tid = 0;
  wr_thread->start(tid);
  rd_thread->start(tid);

  while(1) {
    os_msleep(1000);
  }

  CHECK_EQ(0, buffer_queue_destory(bq));
  return 0;
}

int32_t memory_main(int32_t argc, char *argv[]) {
  bool b_ret = false;
  scoped_ptr<IMemory> pmem(new IMemory(eVCFormatI420, 0));
  b_ret = pmem->allocate(NULL, 128);
  CHECK_EQ(true, b_ret);
  log_verbose("tag" , "allocate 128 bytes success!\n");
  pmem->deallocate();
  scoped_ptr<VideoMemory> pvideo_mem(new VideoMemory(1920, 1080));
  b_ret = pvideo_mem->allocate(NULL, 0);
  CHECK_EQ(true, b_ret);
  log_verbose("tag" , "allocate 1080p video memory success!\n");
  memset(pvideo_mem->memory(0), 0xaa, 1920*1080);
  pvideo_mem->deallocate();
  return 0;
}

int32_t main(int32_t argc, char *argv[]) {
  //return bq_main(argc, argv);
  return memory_main(argc, argv);
}
