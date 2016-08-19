#ifndef CORE_REF_H_
#define CORE_REF_H_

namespace core {

class IRefBase {
public:
  virtual IRefBase() {}

  virtual int32_t add_ref() = 0;
  virtual int32_t release() = 0;
};

} // namespace core

#endif //CORE_REF_H_
