#ifndef CORE_SCOPED_REFPTR_H_
#define CORE_SCOPED_REFPTR_H_

namespace core {

#ifndef NULL
#define NULL (0)
#endif

template <class T>
class scoped_refptr {
 public:
  scoped_refptr() : ptr_(NULL) {
  }

  scoped_refptr(T* p) : ptr_(p) {
    if (ptr_)
      ptr_->add_ref();
  }

  scoped_refptr(const scoped_refptr<T>& r) : ptr_(r.ptr_) {
    if (ptr_)
      ptr_->add_ref();
  }

  template <typename U>
  scoped_refptr(const scoped_refptr<U>& r) : ptr_(r.get()) {
    if (ptr_)
      ptr_->add_ref();
  }

  ~scoped_refptr() {
    if (ptr_)
      ptr_->release();
  }

  T* get() const { return ptr_; }
  operator T*() const { return ptr_; }
  T* operator->() const { return ptr_; }

  // release a pointer.
  // The return value is the current pointer held by this object.
  // If this object holds a NULL pointer, the return value is NULL.
  // After this operation, this object will hold a NULL pointer,
  // and will not own the object any more.
  T* release() {
    T* retVal = ptr_;
    ptr_ = NULL;
    return retVal;
  }

  scoped_refptr<T>& operator=(T* p) {
    // add_ref first so that self assignment should work
    if (p)
      p->add_ref();
    if (ptr_ )
      ptr_->release();
    ptr_ = p;
    return *this;
  }

  scoped_refptr<T>& operator=(const scoped_refptr<T>& r) {
    return *this = r.ptr_;
  }

  template <typename U>
  scoped_refptr<T>& operator=(const scoped_refptr<U>& r) {
    return *this = r.get();
  }

  void swap(T** pp) {
    T* p = ptr_;
    ptr_ = *pp;
    *pp = p;
  }

  void swap(scoped_refptr<T>& r) {
    swap(&r.ptr_);
  }

 protected:
  T* ptr_;
};
}  // namespace core

#endif  // CORE_SCOPED_REFPTR_H_
