#ifndef UTILITY_BYTEORDER_H_
#define UTILITY_BYTEORDER_H_

#include <os_typedefs.h>

#ifdef __cplusplus
extern "C" {
#endif

inline void set8(void* memory, int32_t offset, uint8_t v) {
  *((uint8_t*)memory + offset) = v;
}

inline uint8_t get8(const void* memory, int32_t offset) {
  return *((uint8_t*)memory + offset);
}

inline void be_set16(void* memory, uint16_t v) {
  set8(memory, 0, (uint8_t)(v >> 8));
  set8(memory, 1, (uint8_t)(v >> 0));
}

inline void be_set32(void* memory, uint32_t v) {
  set8(memory, 0, (uint8_t)(v >> 24));
  set8(memory, 1, (uint8_t)(v >> 16));
  set8(memory, 2, (uint8_t)(v >> 8));
  set8(memory, 3, (uint8_t)(v >> 0));
}

inline void be_set64(void* memory, uint64_t v) {
  set8(memory, 0, (uint8_t)(v >> 56));
  set8(memory, 1, (uint8_t)(v >> 48));
  set8(memory, 2, (uint8_t)(v >> 40));
  set8(memory, 3, (uint8_t)(v >> 32));
  set8(memory, 4, (uint8_t)(v >> 24));
  set8(memory, 5, (uint8_t)(v >> 16));
  set8(memory, 6, (uint8_t)(v >> 8));
  set8(memory, 7, (uint8_t)(v >> 0));
}

inline uint16_t be_get16(const void* memory) {
  return (uint16_t)((get8(memory, 0) << 8) |
                             (get8(memory, 1) << 0));
}

inline uint32_t be_get32(const void* memory) {
  return ((uint32_t)(get8(memory, 0)) << 24) |
      ((uint32_t)(get8(memory, 1)) << 16) |
      ((uint32_t)(get8(memory, 2)) << 8) |
      ((uint32_t)(get8(memory, 3)) << 0);
}

inline uint64_t be_get64(const void* memory) {
  return ((uint64_t)(get8(memory, 0)) << 56) |
      ((uint64_t)(get8(memory, 1)) << 48) |
      ((uint64_t)(get8(memory, 2)) << 40) |
      ((uint64_t)(get8(memory, 3)) << 32) |
      ((uint64_t)(get8(memory, 4)) << 24) |
      ((uint64_t)(get8(memory, 5)) << 16) |
      ((uint64_t)(get8(memory, 6)) << 8) |
      ((uint64_t)(get8(memory, 7)) << 0);
}

inline void le_set16(void* memory, uint16_t v) {
  set8(memory, 0, (uint8_t)(v >> 0));
  set8(memory, 1, (uint8_t)(v >> 8));
}

inline void le_set32(void* memory, uint32_t v) {
  set8(memory, 0, (uint8_t)(v >> 0));
  set8(memory, 1, (uint8_t)(v >> 8));
  set8(memory, 2, (uint8_t)(v >> 16));
  set8(memory, 3, (uint8_t)(v >> 24));
}

inline void le_set64(void* memory, uint64_t v) {
  set8(memory, 0, (uint8_t)(v >> 0));
  set8(memory, 1, (uint8_t)(v >> 8));
  set8(memory, 2, (uint8_t)(v >> 16));
  set8(memory, 3, (uint8_t)(v >> 24));
  set8(memory, 4, (uint8_t)(v >> 32));
  set8(memory, 5, (uint8_t)(v >> 40));
  set8(memory, 6, (uint8_t)(v >> 48));
  set8(memory, 7, (uint8_t)(v >> 56));
}

inline uint16_t le_get16(const void* memory) {
  return (uint16_t)((get8(memory, 0) << 0) |
                             (get8(memory, 1) << 8));
}

inline uint32_t le_get32(const void* memory) {
  return ((uint32_t)(get8(memory, 0)) << 0) |
      ((uint32_t)(get8(memory, 1)) << 8) |
      ((uint32_t)(get8(memory, 2)) << 16) |
      ((uint32_t)(get8(memory, 3)) << 24);
}

inline uint64_t le_get64(const void* memory) {
  return ((uint64_t)(get8(memory, 0)) << 0) |
      ((uint64_t)(get8(memory, 1)) << 8) |
      ((uint64_t)(get8(memory, 2)) << 16) |
      ((uint64_t)(get8(memory, 3)) << 24) |
      ((uint64_t)(get8(memory, 4)) << 32) |
      ((uint64_t)(get8(memory, 5)) << 40) |
      ((uint64_t)(get8(memory, 6)) << 48) |
      ((uint64_t)(get8(memory, 7)) << 56);
}

inline bool is_host_big_endian() {
  static const int32_t number = 1;
  return 0 == *(const char*)(&number);
}

#ifdef __cplusplus
} //extern "C"
#endif

#endif  // UTILITY_BYTEORDER_H_
