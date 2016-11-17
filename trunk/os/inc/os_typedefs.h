#ifndef _OS_TYPEDEFS_H_
#define _OS_TYPEDEFS_H_

#if 0
#if defined(WIN32)
  // windows
  #define _OS_WINDOWS

#else
  // linux
  #define _OS_LINUX

#endif
#endif

// basic typedefs
#if defined(_OS_ANDROID)
  // for andorid compile error ‘INT64_C’ was not declared in this scope
  #ifdef __cplusplus
    #define __STDC_CONSTANT_MACROS
    #ifdef _STDINT_H
      #undef _STDINT_H
    #endif
    # include <stdint.h>
  #endif
#elif defined(_OS_LINUX)
  #include <stdint.h>
#elif defined(_OS_WINDOWS) && defined(_MSC_VER)
  // Define C99 equivalent types.
  // Since MSVC doesn't include these headers, we have to write our own
  // version to provide a compatibility layer between MSVC and the WebRTC
  // headers.
  typedef signed char         int8_t;
  typedef signed short        int16_t;
  typedef signed int          int32_t;
  typedef signed long long    int64_t;
  typedef unsigned char       uint8_t;
  typedef unsigned short      uint16_t;
  typedef unsigned int        uint32_t;
  typedef unsigned long long  uint64_t;
#else
  #error "Unknown os, no support typedefs"
#endif

#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#if defined(_OS_ANDROID)
// for sdk >= 21 support monitic clock for android
// FIXME:TODO (jianbo)
#define _OS_CLOCK_REALTIME
#endif

enum OS_ERROR {
  eNoError = 0x0,
};

// clock type
//_OS_CLOCK_REALTIME
//_OS_CLOCK__MONOTONIC

#define GUARDED_BY(x)

#endif //_OS_TYPEDEFS_H_
