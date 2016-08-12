#ifndef _OS_ATOMIC_H_
#define _OS_ATOMIC_H_

#include <cstddef>
#include <os_typedefs.h>

namespace os {

class Atomic
{
private:
  int32_t _value;
public:
  Atomic(int32_t initial_value = 0);
  ~Atomic();

  int32_t operator++();
  int32_t operator--();

  int32_t operator+=(int32_t value);
  int32_t operator-=(int32_t value);

  int32_t value() const;

private:
    // Disable the +/-/= operator
    // since it's unclear what these operations should do.
    Atomic operator+(const Atomic & other);
    Atomic operator-(const Atomic & other);
    Atomic operator=(const Atomic & other);

    // Disallow duplicate
    Atomic(const Atomic &);

    // Checks if |_value| is 32bit aligned.
    inline bool is_32bit_aligned() const {
      return (reinterpret_cast<ptrdiff_t>(&_value) & 3) == 0;
    }
};
}  // namespace os

#endif // _OS_ATOMIC_H_
