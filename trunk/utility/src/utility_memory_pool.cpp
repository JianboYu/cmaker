#include <os_assert.h>
#include <utility_memory_pool.h>

namespace utility {

IMemoryPool *IMemoryPool::Create() {
  return new MemoryPoolImpl();
}

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
  AutoLock lock(_mutex.get());
  int32_t slot_id = bitmap_find_first_zero_bit(_slot_bm);
  if (slot_id < 0)
    return -1;

  MemorySlot *mem_slot = new MemorySlot();
  if (!mem_slot)
    return -2;

  mem_slot->type = eMemoryTypeNormal;
  mem_slot->u.normal.size = size;
  mem_slot->u.normal.usage = usage;
  mem_slot->max_size = max_num;
  mem_slot->actual_size = 0;
  mem_slot->mutex.reset(Mutex::Create());

  _slots[slot_id] = mem_slot;
  bitmap_set_bit(_slot_bm, slot_id, 1);
  *slot = slot_id;
  return 0;
}
int32_t MemoryPoolImpl::create_slot(int32_t *slot, int32_t w, int32_t h,
                                    int32_t w_stride, int32_t h_stride,
                                    int32_t color_fmt, int32_t usage,
                                    int32_t max_num) {
  AutoLock lock(_mutex.get());
  int32_t slot_id = bitmap_find_first_zero_bit(_slot_bm);
  if (slot_id < 0)
    return -1;

  MemorySlot *mem_slot = new MemorySlot();
  if (!mem_slot)
    return -2;

  mem_slot->type = eMemoryTypeVideo;
  mem_slot->u.video.width = w;
  mem_slot->u.video.height = h;
  mem_slot->u.video.w_stride = w_stride;
  mem_slot->u.video.h_stride = h_stride;
  mem_slot->u.video.color_fmt = color_fmt;
  mem_slot->u.video.usage = usage;
  mem_slot->max_size = max_num;
  mem_slot->actual_size = 0;
  mem_slot->mutex.reset(Mutex::Create());

  _slots[slot_id] = mem_slot;
  bitmap_set_bit(_slot_bm, slot_id, 1);
  *slot = slot_id;

  return 0;
}
int32_t MemoryPoolImpl::destroy_slot(int32_t slot) {
  AutoLock lock(_mutex.get());
  std::map<int32_t, MemorySlot*>::iterator it = _slots.find(slot);
  if (it == _slots.end())
    return -1;

  MemorySlot *pSlot = it->second;
  pSlot->mutex->lock();
  if (pSlot->mem.size() != pSlot->mem_free.size()) {
    // not all mem returned
    return -2;
  }

  std::vector<IMemory*>::iterator mem_it = pSlot->mem.begin();
  while(mem_it != pSlot->mem.end()) {
    IMemory *pMem = *mem_it;
    pSlot->mem.erase(mem_it);
    pMem->deallocate();
    delete pMem;
    mem_it = pSlot->mem.begin();
  }
  _slots.erase(it);
  pSlot->mutex->unlock();
  delete pSlot;

  bitmap_set_bit(_slot_bm, slot, 0);
  return 0;
}
IMemory *MemoryPoolImpl::get(int32_t slot) {
  IMemory *pMem = NULL;
  std::map<int32_t, MemorySlot*>::iterator it = _slots.find(slot);
  if (it == _slots.end())
    return NULL;

  MemorySlot *pSlot = it->second;
  AutoLock mem_lock(pSlot->mutex.get());
  if (pSlot->mem_free.size() > 0) {
    pMem = pSlot->mem_free.front();
    pSlot->mem_free.pop_front();
    pSlot->mem_used.push_back(pMem);
    return pMem;
  }
  if (pSlot->actual_size >= pSlot->max_size)
    return NULL;

  uint32_t size = 0;
  switch(pSlot->type) {
    case eMemoryTypeNormal:
    pMem = new IMemory(eMemoryTypeNormal, pSlot->u.normal.usage);
    size = pSlot->u.normal.size;
    break;
    case eMemoryTypeVideo:
    pMem = new VideoMemory(pSlot->u.video.width,
                           pSlot->u.video.height,
                           pSlot->u.video.color_fmt,
                           pSlot->u.video.w_stride,
                           pSlot->u.video.h_stride);
    break;
    default:
    break;
  }
  // All memory pool use internal buffer allocate
  pMem->allocate(NULL, size);

  ++pSlot->actual_size;
  pSlot->mem.push_back(pMem);
  pSlot->mem_used.push_back(pMem);
  return pMem;
}
int32_t MemoryPoolImpl::put(int32_t slot, const IMemory *mem) {
  std::map<int32_t, MemorySlot*>::iterator it = _slots.find(slot);
  if (it == _slots.end())
    return -1;

  MemorySlot *pSlot = it->second;
  AutoLock mem_lock(pSlot->mutex.get());

  std::list<IMemory*>::iterator mem_used_it = pSlot->mem_used.begin();
  while(mem_used_it != pSlot->mem_used.end()) {
    if (mem == *mem_used_it)
        break;
    ++mem_used_it;
  }
  if (mem_used_it == pSlot->mem_used.end())
    return -2;
  pSlot->mem_used.erase(mem_used_it);

  pSlot->mem_free.push_back(const_cast<IMemory*>(mem));
  return 0;
}

} //namespace utility
