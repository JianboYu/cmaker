#include <os_time.h>

#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  #include <time.h>
  #include <sys/time.h>
  #include <unistd.h>
#else
  #error "Unknown os"
#endif


#ifdef __cplusplus
extern "C" {
#endif

int64_t os_get_realtime() {
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  struct timeval tv;
  gettimeofday(&tv, NULL);
  int64_t now = tv.tv_sec * 1000000ll + tv.tv_usec;
  now += ((70ll * 365 + 17) * 24) * 60 * 60 * 1000000ll;
  uint64_t hi = now / 1000000ll;
  uint64_t lo = ((1ll << 32) * (now % 1000000ll)) / 1000000ll;
  return (hi << 32) | lo;
#else
  #error "Unknown os"
#endif
  return -1;
}

int64_t os_get_systime() {
  return os_get_realtime();
}

int64_t os_get_monitictime() {
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  int64_t ret = 1000000000LL * static_cast<int64_t>(ts.tv_sec) +
    static_cast<int64_t>(ts.tv_nsec);
  return ret;
#else
  #error "Unknown os"
#endif
  return -1;
}

int32_t os_usleep(int32_t us) {
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  return usleep(us);
#else
  #error "Unknown os"
#endif
  return -1;
}

int32_t os_msleep(int32_t ms) {
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  return usleep(ms * 1000);
#else
  #error "Unknown os"
#endif
  return -1;
}

int32_t os_sleep(int32_t s) {
#if defined(_OS_LINUX) || defined(_OS_ANDROID)
  return sleep(s);
#else
  #error "Unknown os"
#endif
  return -1;
}

#ifdef __cplusplus
}
#endif


