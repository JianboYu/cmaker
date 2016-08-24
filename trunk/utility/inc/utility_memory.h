#ifndef _UTILITY_MEMORY_H_
#define _UTILITY_MEMORY_H_

#include <os_atomic.h>
#include <os_mutex.h>
#include <core_ref.h>
#include <core_scoped_ptr.h>

namespace utility {

using namespace os;
using namespace core;

enum IMemoryType {
  eMemoryTypeNormal = 0,
  eMemoryTypeVideo = 1,
  eMemoryTypeAudio = 2
};

enum VideoColorFormat {
  eVCFormatI420 = 1,
  eVCFormatNV12 = 2,
  eVCFormatNV21 = 3,
};

class IMemory;
class IMemoryObserver {
public:
  virtual ~IMemoryObserver() {}

  virtual void signal_memory_release(IMemory *memory) = 0;
private:
  IMemoryObserver(const IMemoryObserver &);
  IMemoryObserver & operator=(const IMemoryObserver&);
};

class IMemory : public IRefBase {
private:
  Atomic _refs;
  IMemoryObserver *_obs;
  int32_t _type;
  uint32_t _usage;
  uint8_t *_mem;
  uint32_t _size;
  uint8_t *_mem_internal;
  scoped_ptr<Mutex> _mutex;
public:
  IMemory(int32_t type, uint32_t usage = 0);
  virtual ~IMemory();

  virtual bool allocate(uint8_t *addr, uint32_t size);
  virtual void deallocate();

  virtual uint8_t * ptr();
  virtual uint32_t size() const;

  virtual int32_t type() const;
  virtual uint8_t *attribute(int32_t & size) { return NULL;}

  // IRefBase cls
  virtual int32_t add_ref();
  virtual int32_t release();

  virtual int32_t set_observer(IMemoryObserver* obs);
};

class VideoMemory : public IMemory {
private:
  uint32_t _w;
  uint32_t _h;
  int32_t _fmt;
  uint32_t _w_stride;
  uint32_t _h_stride;
  uint32_t _strided_size;
  int32_t _timestamp;
  int64_t _dts;
  int64_t _pts;
public:
  VideoMemory(uint32_t w,
              uint32_t h,
              int32_t fmt = eVCFormatI420,
              uint32_t w_stride = 0,
              uint32_t h_stride = 0);
  virtual ~VideoMemory();

  virtual bool allocate(uint8_t *addr, uint32_t size);
  virtual void deallocate();

  virtual uint32_t width() const;
  virtual uint32_t height() const;
  virtual uint32_t wstride() const;
  virtual uint32_t hstride() const;
  virtual int32_t  format() const;

  virtual uint8_t *memory(int32_t plane);

  virtual int32_t timestamp() const;
  virtual int64_t dts() const;
  virtual int64_t pts() const;
  virtual int32_t set_timestamp(int32_t ts);
  virtual int64_t set_dts(int64_t dts);
  virtual int64_t set_pts(int64_t pts);
private:
  int32_t calc_size(uint32_t w, uint32_t h,
                    int32_t fmt,
                    uint32_t w_stride, uint32_t h_stride);
};

class MemoryPool {
public:
  static MemoryPool *Create();
  virtual ~MemoryPool() {}

  virtual int32_t create_slot(int32_t *slot, int32_t size, int32_t usage) = 0;
  virtual int32_t create_slot(int32_t *slot, int32_t w, int32_t h,
                              int32_t w_stride, int32_t h_stride,
                              int32_t color_fmt) = 0;
  virtual int32_t destroy_slot(int32_t slot) = 0;

  virtual IMemory *get(int32_t slot) = 0;
  virtual void dump() = 0;
};

} //namespace utility
#endif //_UTILITY_MEMORY_H_
