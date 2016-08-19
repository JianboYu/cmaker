#include <map>
#include <list>
#include <os_typedefs.h>
#include <os_mutex.h>
#include <os_cond.h>
#include <os_assert.h>
#include <utility_buffer_queue.h>
#include <core_scoped_ptr.h>

#ifdef __cplusplus
extern "C" {
#endif

static const int32_t eq = 0;
static const int32_t fq = 1;

typedef struct bq {
  std::map<int32_t, void *> buffer;
  std::list<int32_t> q[2];
  core::scoped_ptr<os::Mutex> mutex[2];
  core::scoped_ptr<os::Cond> cond[2];

  bq() {
    mutex[eq].reset(os::Mutex::Create());
    cond[eq].reset(os::Cond::Create());
    mutex[fq].reset(os::Mutex::Create());
    cond[fq].reset(os::Cond::Create());
  }
}bq;

int32_t buffer_queue_create(buffer_queue_handle *hndl,
                            void *array_ptr[],
                            int32_t size) {
  bq * obj = new bq();
  if (!obj)
    return -1;

  for (int i = 0; i < size; ++i) {
    CHECK(array_ptr[i]);
    obj->buffer[i] = array_ptr[i];
    obj->q[eq].push_back(i);
  }

  *hndl = obj;
  return 0;
}

int32_t buffer_queue_destory(buffer_queue_handle hndl) {
  bq * obj = static_cast<bq *>(hndl);
  CHECK(obj);
  delete obj;
  return 0;
}

int32_t buffer_queue_get_empty_timeout(buffer_queue_handle hndl,
                                       int32_t *array_idx,
                                       uint32_t ms) {
  int32_t ret = -1;
  bq * obj = static_cast<bq *>(hndl);
  obj->mutex[eq]->lock();

  if (obj->q[eq].size() > 0) {
    *array_idx = obj->q[eq].front();
    obj->q[eq].pop_front();
    ret = 0;
    goto _out;
  }

  if (BQ_TIMEOUT_INFINIT == ms) {
    obj->cond[eq]->wait(obj->mutex[eq].get());
  } else if (ms > 0) {
    obj->cond[eq]->wait_timeout(obj->mutex[eq].get(), ms);
  } else {
    // ms == 0
  }

  if (obj->q[eq].size() > 0) {
    *array_idx = obj->q[eq].front();
    obj->q[eq].pop_front();
    ret = 0;
  }

_out:
  obj->mutex[eq]->unlock();
  return ret;
}
int32_t buffer_queue_put_full(buffer_queue_handle hndl,
                              int32_t array_idx) {
  bq * obj = static_cast<bq *>(hndl);
  obj->mutex[fq]->lock();
  obj->q[fq].push_back(array_idx);
  obj->mutex[fq]->unlock();
  obj->cond[fq]->signal();
  return 0;
}

int32_t buffer_queue_get_full_timeout(buffer_queue_handle hndl,
                                      int32_t *array_idx,
                                      int32_t ms) {
  int32_t ret = -1;
  bq * obj = static_cast<bq *>(hndl);
  obj->mutex[fq]->lock();

  if (obj->q[fq].size() > 0) {
    *array_idx = obj->q[fq].front();
    obj->q[fq].pop_front();
    ret = 0;
    goto _out;
  }

  if ((int32_t)BQ_TIMEOUT_INFINIT == ms) {
    obj->cond[fq]->wait(obj->mutex[fq].get());
  } else if (ms > 0) {
    obj->cond[fq]->wait_timeout(obj->mutex[fq].get(), ms);
  } else {
    // ms == 0
  }

  if (obj->q[fq].size() > 0) {
    *array_idx = obj->q[fq].front();
    obj->q[fq].pop_front();
    ret = 0;
  }

_out:
  obj->mutex[fq]->unlock();
  return ret;
}

int32_t buffer_queue_put_empty(buffer_queue_handle hndl,
                               int32_t array_idx) {
  bq * obj = static_cast<bq *>(hndl);
  obj->mutex[eq]->lock();
  obj->q[eq].push_back(array_idx);
  obj->mutex[eq]->unlock();
  obj->cond[eq]->signal();
  return 0;
}


#ifdef __cplusplus
}
#endif



