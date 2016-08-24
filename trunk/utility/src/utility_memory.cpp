#include <string.h>

#include <os_assert.h>
#include <os_log.h>
#include <utility_memory.h>

namespace utility {

// IMemory cls
IMemory::IMemory(int32_t type,  uint32_t usage) :
                _refs(0),
                _obs(NULL),
                _type(type),
                _usage(usage),
                _mem(NULL),
                _size(0),
                _mem_internal(NULL) {
  _mutex.reset(Mutex::Create());
}

IMemory::~IMemory() {
  if (_mem_internal)
    delete _mem_internal;
  _mem_internal = NULL;
}

bool IMemory::allocate(uint8_t *addr, uint32_t size) {
  CHECK(!_mem);
  _mem = addr;
  _size = size;
  if (!_mem && _size > 0) {
    log_verbose("IMemory", "allocate new size(%d) bytes\n", _size);
    _mem = _mem_internal = new uint8_t[_size];
    CHECK(_mem);
    memset(_mem, 0x0, _size);
  }
  return true;
}
void IMemory::deallocate() {
  if (_mem_internal)
    delete _mem_internal;
  _mem_internal = NULL;
  _mem = NULL;
}

uint8_t * IMemory::ptr() {
  return _mem;
}

uint32_t IMemory::size() const {
  return _size;
}

int32_t IMemory::type() const {
  return _type;
}

int32_t IMemory::add_ref() {
  AutoLock cs(_mutex.get());
  ++_refs;
  return 0;
}
int32_t IMemory::release() {
  AutoLock cs(_mutex.get());
  --_refs;

  if (_refs.value() > 0)
    return 0;

  if (!_obs) {
    delete this;
  } else {
    _obs->signal_memory_release(this);
  }
  return 0;
}

int32_t IMemory::set_observer(IMemoryObserver* obs) {
  CHECK(_obs && obs);
  _obs = obs;
  return 0;
}

// VideoMemory cls
VideoMemory::VideoMemory(uint32_t w, uint32_t h,
                         int32_t fmt,
                         uint32_t w_stride,
                         uint32_t h_stride):
                         IMemory(eMemoryTypeVideo, 0),
                         _w(w),
                         _h(h),
                         _fmt(fmt),
                         _w_stride(w_stride),
                         _h_stride(h_stride)
{
  if (0 == _w_stride)
    _w_stride = _w;
  if (0 == _h_stride)
    _h_stride = _h;

  CHECK_GE(_w_stride, _w);
  CHECK_GE(_h_stride, _h);
  _strided_size = calc_size(_w, _h, _fmt, _w_stride, _h_stride);
}
VideoMemory::~VideoMemory() {
}
bool VideoMemory::allocate(uint8_t *addr, uint32_t size) {
  if (addr && size >= _strided_size) {
    return IMemory::allocate(addr, size);
  }

  log_verbose("VideoMemory", "%dx%d strided size(%d) bytes\n",
              _w, _h, _strided_size);
  return IMemory::allocate(NULL, _strided_size);
}
void VideoMemory::deallocate() {
  return IMemory::deallocate();
}
uint32_t VideoMemory::width() const {
  return _w;
}
uint32_t VideoMemory::height() const {
  return _h;
}
uint32_t VideoMemory::wstride() const {
  return _w_stride;
}
uint32_t VideoMemory::hstride() const {
  return _h_stride;
}
int32_t  VideoMemory::format() const {
  return _fmt;
}
uint8_t *VideoMemory::memory(int32_t plane) {
  return ptr();
}
int32_t VideoMemory::timestamp() const {
  return _timestamp;
}
int64_t VideoMemory::dts() const {
  return _dts;
}
int64_t VideoMemory::pts() const {
  return _pts;
}
int32_t VideoMemory::set_timestamp(int32_t ts) {
  return (_timestamp = ts);
}
int64_t VideoMemory::set_dts(int64_t dts) {
  return (_dts = dts);
}
int64_t VideoMemory::set_pts(int64_t pts) {
  return (_pts = pts);
}

int32_t VideoMemory::calc_size(uint32_t w, uint32_t h,
                               int32_t fmt,
                               uint32_t w_stride,
                               uint32_t h_stride) {
  int32_t ret = 0;
  switch(fmt) {
  case eVCFormatI420:
  case eVCFormatNV12:
  case eVCFormatNV21:
    ret = w_stride * h_stride * 3 / 2;
    break;
  default:
    ret = w_stride * h_stride * 2;
    break;
  }
  return ret;
}

} //namespace utility
