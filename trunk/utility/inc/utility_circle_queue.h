#ifndef _UTILITY_CIRCLE_QUEUE_H_
#define _UTILITY_CIRCLE_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef void* cirq_handle;

int32_t cirq_create(cirq_handle *hndl, uint32_t size);
int32_t cirq_destory(cirq_handle hndl);

int32_t cirq_enqueue(cirq_handle hndl, void *data);
int32_t cirq_dequeue(cirq_handle hndl, void **data);

int32_t cirq_empty(cirq_handle hndl);
int32_t cirq_full(cirq_handle hndl);

#ifdef __cplusplus
}
#endif

#endif //_UTILITY_CIRCLE_QUEUE_H_
