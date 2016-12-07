#ifndef UTILITY_COMMON_H_
#define UTILITY_COMMON_H_

#include <os_typedefs.h>

#define CELING(val, n) (val & ~(n-1))
#define ALIGN(val, n) ((val + n - 1) & ~(n - 1))

#define CELING16(val) CELING(val, 16)
#define CELING32(val) CELING(val, 32)

#define ALIGN16(val) ALIGN(val, 16)
#define ALIGN32(val) ALIGN(val, 32)

#define ARRARY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
} //extern "C"
#endif

#endif  // UTILITY_COMMON_H_
