#include <string.h>
#include <os_typedefs.h>
#include <os_mutex.h>
#include <os_assert.h>
#include <os_thread.h>
#include <os_time.h>
#include <core_scoped_ptr.h>
#include <utility_buffer_queue.h>
#include <utility_circle_queue.h>
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

bool cq_wr_loop(void *ctx) {
  static void *array_ptr[5] = {(void*)0x11,(void*)0x12,(void*)0x13,(void*)0x14,(void*)0x15};
  static int32_t s_times = 0;
  cirq_handle cq = static_cast<cirq_handle>(ctx);
  if (0 != cirq_enqueue(cq, array_ptr[s_times % 5])){
    return true;
  }

  os_msleep(40);
  log_verbose("tag" , "cq wr data: %d\n", array_ptr[s_times % 5]);
  ++s_times;
  return true;
}

bool cq_rd_loop(void *ctx) {
  cirq_handle cq = static_cast<cirq_handle>(ctx);
  int32_t *data = NULL;
  if (0 != cirq_dequeue(cq, (void**)&data)){
    return true;
  }
  os_msleep(40);
  log_verbose("tag" , "\t\tcq rd data: %d\n", data);
  return true;
}

int32_t cq_main(int32_t argc, char *argv[]) {
  cirq_handle cq;
  CHECK_EQ(0, cirq_create(&cq, 5));

  scoped_ptr<Thread> wr_thread(Thread::Create(cq_rd_loop, cq));
  scoped_ptr<Thread> rd_thread(Thread::Create(cq_wr_loop, cq));
  uint32_t tid = 0;
  wr_thread->start(tid);
  rd_thread->start(tid);

  while(1) {
    os_msleep(1000);
  }

  CHECK_EQ(0, cirq_destory(cq));
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

int32_t memory_pool_main(int32_t argc, char *argv[]) {
  int32_t usage = 0x13;
  scoped_ptr<IMemoryPool> pPool(IMemoryPool::Create());
  int32_t slot[6] = {-1, -1, -1, -1, -1, -1};

  log_verbose("tag" , "MemoryPool: %p\n", pPool.get());
  for (int32_t i = 0; i < 3; ++i) {
    CHECK_EQ(0, pPool->create_slot(&slot[i*2],  128 * 64 * i + 128,
                        usage, 2 * i + 2));
    log_verbose("tag" , "slot[%d]: %d created\n", i*2, slot[i*2]);
    CHECK_EQ(0, pPool->create_slot(&slot[i*2 + 1], 1920, 1080, 0, 0,
                              eVCFormatI420, usage,
                              4 * i + 2));
    log_verbose("tag" , "slot[%d]: %d created\n", i*2+1, slot[i*2+1]);
  }

  IMemory *pMem = pPool->get(slot[0]);
  log_verbose("tag" , "get pMem: %p\n", pMem);
  CHECK_EQ(0, pPool->put(slot[0], pMem));
  log_verbose("tag" , "put pMem: %p\n", pMem);
  pMem = pPool->get(slot[0]);
  log_verbose("tag" , "get pMem: %p\n", pMem);
  CHECK(pMem->ptr());
  CHECK_EQ(pMem->size(), 128);

  CHECK_EQ(0, pPool->put(slot[0], pMem));
  log_verbose("tag" , "put pMem: %p\n", pMem);

  IMemory *pMemVideo = pPool->get(slot[1]);
  log_verbose("tag" , "get video pMem: %p\n", pMemVideo);
  CHECK_EQ(0, pPool->put(slot[1], pMemVideo));
  pMemVideo = pPool->get(slot[1]);
  log_verbose("tag" , "get video pMem: %p\n", pMemVideo);
  CHECK(pMemVideo->ptr());
  CHECK_EQ(pMemVideo->size(), 3110400);
  CHECK_EQ(0, pPool->put(slot[1], pMemVideo));

  for (int32_t i = 0; i < 6; ++i) {
    CHECK_EQ(0, pPool->destroy_slot(slot[i]));
  }
  log_verbose("tag" , "MemPool test pass!!\n");
  return 0;
}

static void usage() {
  logv("Usage:\n");
  logv("./utest_utility fun_name [options]\n");
  logv("fun_name as following:\n");
  logv("1. cq\t\tcircle queue\n");
  logv("2. bq\t\tbuffer queue\n");
  logv("3. mem\t\tmemory\n");
  logv("4. mempool\tmemory pool\n");
}

int32_t main(int32_t argc, char *argv[]) {
  if (argc < 2) {
    usage();
    exit(0);
  }
  if (!strcmp(argv[1], "cq")) {
    return cq_main(argc-2, &argv[2]);
  } else if (!strcmp(argv[1], "bq")) {
    return bq_main(argc-2, &argv[2]);
  } else if (!strcmp(argv[1], "mem")) {
    return memory_main(argc-2, &argv[2]);
  } else if (!strcmp(argv[1], "mempool")) {
    return memory_pool_main(argc-2, &argv[2]);
  } else {
    usage();
  }
  return 0;
}
