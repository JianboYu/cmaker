#ifndef _UTILITY_MEMORY_H_
#define _UTILITY_MEMORY_H_

#include <os_atomic.h>
#include <core_ref.h>

namespace utility {

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

  void signal_memory_release(IMemory *memory) = 0;
private:
  IMemoryObserver(const IMemoryObserver &);
  IMemoryObserver & operator=(const IMemoryObserver&);
};

class IMemory : public IRefBase {
private:
  Atomic _refs;
  IMemoryObserver *_obs;
public:
  IMemory(uint8_t *addr, uint32_t size, int32_t type, uint32_t usage = 0);
  virtual ~IMemory() {}

  virtual uint8_t * ptr() = 0;
  virtual uint32_t size() const = 0;

  virtual int32_t type() const = 0;
  virtual uint8_t *attribute(int32_t & size) { return NULL;}

  // IRefBase cls
  virtual int32_t add_ref();
  virtual int32_t release();

  virtual int32_t set_observer(IMemoryObserver* obs);
};

class VideoMemory : public IMemory {
private:
  int32_t _timestamp;
  int64_t _dts;
  int64_t _pts;
public:
  VideoMemory(uint32_t w,
              uint32_t h,
              int32_t fmt,
              uint32_t w_stride = 0,
              uint32_t h_stride = 0);
  virtual ~VideoMemory();

  uint32_t width() const;
  uint32_t height() const;
  uint32_t wstride() const;
  uint32_t hstride() const;
  int32_t  format() const;

  uint8_t *memory(int32_t plane);

  int32_t timestamp() const;
  int64_t dts() const;
  int64_t pts() const;
  int32_t set_timestamp(int32_t ts);
  int64_t set_dts(int64_t dts);
  int64_t set_pts(int64_t pts);
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
