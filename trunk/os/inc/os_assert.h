#ifndef _OS_ASSERT_H_
#define _OS_ASSERT_H_

#include <os_typedefs.h>
#include <os_log.h>

namespace os {

template<class t1, class t2>
std::string* MakeCheckOpString(const t1& v1, const t2& v2, const char* names) {
  std::ostringstream ss;
  ss << names << " (" << v1 << " vs " << v2 << ")";
  std::string* msg = new std::string(ss.str());
  return msg;
}

#define CHECK(condition)                                                      \
  if (!condition) {                                                           \
    os::FatalLog(__FILE__, __LINE__).stream()                                 \
      << "Check failed--\"" #condition "\"--" << std::endl;                   \
  }

#define DEFINE_CHECK_OP_IMPL(name, op)                                        \
  template <class t1, class t2>                                               \
  inline std::string* Check##name##Impl(const t1& v1, const t2& v2,           \
                                        const char* names) {                  \
    if (v1 op v2) return NULL;                                                \
    else return os::MakeCheckOpString(v1, v2, names);                         \
  }                                                                           \

DEFINE_CHECK_OP_IMPL(EQ, ==)
DEFINE_CHECK_OP_IMPL(NE, !=)
DEFINE_CHECK_OP_IMPL(LE, <=)
DEFINE_CHECK_OP_IMPL(LT, < )
DEFINE_CHECK_OP_IMPL(GE, >=)
DEFINE_CHECK_OP_IMPL(GT, > )
#undef DEFINE_CHECK_OP_IMPL

#define CHECK_OP(name, op, val1, val2)                      \
  if (std::string* _result =                                \
    os::Check##name##Impl((val1), (val2),                   \
                             #val1 " " #op " " #val2))      \
    os::FatalLog(__FILE__, __LINE__, _result).stream()

#define CHECK_EQ(val1, val2) CHECK_OP(EQ, ==, val1, val2)
#define CHECK_NE(val1, val2) CHECK_OP(NE, !=, val1, val2)
#define CHECK_LE(val1, val2) CHECK_OP(LE, <=, val1, val2)
#define CHECK_LT(val1, val2) CHECK_OP(LT, < , val1, val2)
#define CHECK_GE(val1, val2) CHECK_OP(GE, >=, val1, val2)
#define CHECK_GT(val1, val2) CHECK_OP(GT, > , val1, val2)
} // namespace os

#endif //_OS_ASSERT_H_
