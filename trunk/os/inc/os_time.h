#ifndef _OS_TIME_H_
#define _OS_TIME_H_

#include <os_typedefs.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t os_get_systime();
int64_t os_get_realtime();
int64_t os_get_monitictime();

int32_t os_usleep(int32_t us);
int32_t os_msleep(int32_t ms);
int32_t os_sleep(int32_t s);

#ifdef __cplusplus
}
#endif

#endif //_OS_TIME_H_
