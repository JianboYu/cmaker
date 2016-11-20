#include <string.h>
#include <os_mutex.h>
#include <core_scoped_ptr.h>
#include <utility_bitmap.h>

using namespace os;
using namespace core;

namespace utility {

class BitsMap {
private:
  #define INDEXSFT (3)
  #define BYTEBITS (8)
  #define MASK     (0x7)
  scoped_array<uint8_t> _bits;
  uint32_t _num;
  scoped_ptr<Mutex> _mutex;

public:
  BitsMap(uint32_t num) : _num(num>>3) {
    _bits.reset(new uint8_t[_num]);
    _mutex.reset(Mutex::Create());
  }

  void set_bit(uint32_t idx, uint32_t data) {
    AutoLock cs(_mutex.get());
    if (idx > (_num << 3))
      return;
    if (data == 0) {
      _bits[idx >> INDEXSFT] &= ~( 1 << (BYTEBITS -1 - (idx & MASK)));
    } else {
      _bits[idx >> INDEXSFT] |= (1 << (BYTEBITS -1 - (idx & MASK)));
    }
  }
  void clear() {
    AutoLock cs(_mutex.get());
    memset(_bits.get(), 0x0, _num);
  }
  int32_t get_bit(uint32_t idx) {
    AutoLock cs(_mutex.get());
    return (_bits[idx >> INDEXSFT] & (1 << (BYTEBITS -1 - (idx & MASK))));
  }
  int32_t first_zero_bit() {
    AutoLock cs(_mutex.get());
    uint32_t i = 0, test_bits = 0;
    for (; i < _num; i++) {
      uint32_t j = 0;
      for (; j < BYTEBITS; j++) {
        uint32_t index = i*BYTEBITS+j;
        if (!get_bit(index)) {
          return test_bits;
        }
        test_bits++;
      }
    }
    return -1;
  }
  int32_t is_clear() {
    AutoLock cs(_mutex.get());
    uint32_t i = 0;
    for (; i < _num; i++) {
      uint32_t j = 0;
      for (; j < BYTEBITS; j++) {
        uint32_t index = i*BYTEBITS+j;
        if (get_bit(index)) {
          return -1;
        }
      }
    }
    return 0;
  }
};

extern "C" {
hbitmap bitmap_create(uint32_t bits) {
  return (hbitmap)(new BitsMap(bits));
}
void bitmap_destroy(hbitmap bm) {
  BitsMap *pBM = static_cast<BitsMap*>(bm);
  if (pBM)
    delete pBM;
}

void bitmap_set_bit(hbitmap bm, uint32_t idx, int32_t data) {
  BitsMap *pBM = static_cast<BitsMap*>(bm);
  if (!pBM)
    return;
  pBM->set_bit(idx, data);
}
void bitmap_clear(hbitmap bm) {
  BitsMap *pBM = static_cast<BitsMap*>(bm);
  if (!pBM)
    return;
  pBM->clear();
}
int32_t bitmap_find_first_zero_bit(hbitmap bm) {
  BitsMap *pBM = static_cast<BitsMap*>(bm);
  if (!pBM)
    return -1;
  return pBM->first_zero_bit();
}
int32_t bitmap_get_bit(hbitmap bm, uint32_t idx) {
  BitsMap *pBM = static_cast<BitsMap*>(bm);
  if (!pBM)
    return -1;
  return pBM->get_bit(idx);
}
int32_t bitmap_is_clear(hbitmap bm) {
  BitsMap *pBM = static_cast<BitsMap*>(bm);
  if (!pBM)
    return -1;
  return pBM->is_clear();
}
} //extern "C"

} //namespace utility
