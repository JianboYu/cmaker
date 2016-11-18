#include <os_assert.h>
#include <utility_memory_pool.h>

namespace utility {

#define MEM_POOL_MAX_SLOT_NUM (32)
MemoryPoolImpl::MemoryPoolImpl() : _slot_ids(0) {
  _mutex.reset(Mutex::Create());
  _slot_bm = bitmap_create(MEM_POOL_MAX_SLOT_NUM);
}

MemoryPoolImpl::~MemoryPoolImpl() {
  bitmap_destroy(_slot_bm);
}

int32_t MemoryPoolImpl::create_slot(int32_t *slot, int32_t size,
                                    int32_t usage, int32_t max_num) {
  return 0;
}
int32_t MemoryPoolImpl::create_slot(int32_t *slot, int32_t w, int32_t h,
                                    int32_t w_stride, int32_t h_stride,
                                    int32_t color_fmt, int32_t usage,
                                    int32_t max_num) {
  return 0;
}
int32_t MemoryPoolImpl::destroy_slot(int32_t slot) {
  return 0;
}
IMemory *MemoryPoolImpl::get(int32_t slot) {
  return NULL;
}
int32_t MemoryPoolImpl::put(int32_t slot, const IMemory *mem) {
  return 0;
}

} //namespace utility
