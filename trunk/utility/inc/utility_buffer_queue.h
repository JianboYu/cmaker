#ifndef _UTILITY_BUFFER_QUEUE_H_
#define _UTILITY_BUFFER_QUEUE_H_

#include <os_typedefs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BQ_TIMEOUT_INFINIT (0xffffffff)

typedef void* buffer_queue_handle;

int32_t buffer_queue_create(buffer_queue_handle *hndl,
                            void *array_ptr[],
                            int32_t size);
int32_t buffer_queue_destory(buffer_queue_handle hndl);

int32_t buffer_queue_get_empty_timeout(buffer_queue_handle hndl,
                                       int32_t *array_idx,
                                       uint32_t ms);
int32_t buffer_queue_put_full(buffer_queue_handle hndl,
                              int32_t array_idx);

int32_t buffer_queue_get_full_timeout(buffer_queue_handle hndl,
                                      int32_t *array_idx,
                                      int32_t ms);
int32_t buffer_queue_put_empty(buffer_queue_handle hndl,
                               int32_t array_idx);

#ifdef __cplusplus
}
#endif

#endif //_UTILITY_BUFFER_QUEUE_H_
