#include <assert.h>
#include <os_atomic.h>

namespace os {

Atomic::Atomic(int32_t initial_value): _value(initial_value) {
  assert(is_32bit_aligned());
}
Atomic::~Atomic() {
}

int32_t Atomic::operator++() {
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  return __sync_fetch_and_add(&_value, 1) + 1;
#elif defined(_OS_WINDOWS)
  #error "Un-support for windows"
#else
  #error "Unknown os"
#endif
}
int32_t Atomic::operator--() {
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  return __sync_fetch_and_sub(&_value, 1) - 1;
#elif defined(_OS_WINDOWS)
  #error "Un-support for windows"
#else
  #error "Unknown os"
#endif

}

int32_t Atomic::operator+=(int32_t value) {
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  int32_t ret = __sync_fetch_and_add(&_value, value);
  ret += value;
  return ret;
#elif defined(_OS_WINDOWS)
  #error "Un-support for windows"
#else
  #error "Unknown os"
#endif
}

int32_t Atomic::operator-=(int32_t value) {
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  int32_t ret = __sync_fetch_and_sub(&_value, value);
  ret -= value;
  return ret;
#elif defined(_OS_WINDOWS)
  #error "Un-support for windows"
#else
  #error "Unknown os"
#endif
}

int32_t Atomic::value() const {
  return _value;
}

} //namespace os
