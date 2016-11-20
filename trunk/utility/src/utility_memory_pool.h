#ifndef _UTILITY_MEMORY_POOL_H_
#define _UTILITY_MEMORY_POOL_H_

#include <vector>
#include <map>
#include <list>
#include <os_mutex.h>
#include <utility_bitmap.h>
#include <utility_memory.h>

namespace utility {

class MemoryPoolImpl : public IMemoryPool {
private:
  struct MemorySlot {
    IMemoryType type;
    union {
      struct{
        int32_t size;
        int32_t usage;
      }normal;
      struct{
        int32_t width;
        int32_t height;
        int32_t w_stride;
        int32_t h_stride;
        int32_t color_fmt;
        int32_t usage;
      }video;
      struct{
      }audio;
    }u;
    std::vector<IMemory*> mem;
    std::list<IMemory*> mem_used;
    std::list<IMemory*> mem_free;
    scoped_ptr<Mutex> mutex;
    int32_t max_size;
    int32_t actual_size;
  };
  std::map<int32_t, MemorySlot*> _slots;
  uint32_t _slot_ids;
  hbitmap _slot_bm;
  scoped_ptr<Mutex> _mutex;

public:
  MemoryPoolImpl();
  virtual ~MemoryPoolImpl();

  virtual int32_t create_slot(int32_t *slot, int32_t size,
                              int32_t usage, int32_t max_num);
  virtual int32_t create_slot(int32_t *slot, int32_t w, int32_t h,
                              int32_t w_stride, int32_t h_stride,
                              int32_t color_fmt, int32_t usage,
                              int32_t max_num);
  virtual int32_t destroy_slot(int32_t slot);

  virtual IMemory *get(int32_t slot);
  virtual int32_t put(int32_t slot, const IMemory *mem);
};

} //namespace utility

#endif //_UTILITY_MEMORY_POOL_H_
