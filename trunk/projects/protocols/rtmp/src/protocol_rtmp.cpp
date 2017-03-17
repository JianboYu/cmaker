#include <stdlib.h>
#include <protocol_rtmp.h>
#include <srs_librtmp.hpp>

#ifdef __cplusplus
extern "C" {
#endif

int32_t protocol_rtmp_version_major() {
  return srs_version_major();
}
int32_t protocol_rtmp_version_minor() {
  return srs_version_minor();
}
int32_t protocol_rtmp_version_revision() {
  return srs_version_revision();
}

typedef struct protocol_rtmp_instance {
  srs_rtmp_t srs_rtmp;
}protocol_rtmp_instance;

protocol_rtmp_t protocol_rtmp_create(const char* url) {
  srs_rtmp_t srs_rtmp = srs_rtmp_create(url);
  if (!srs_rtmp)
    return NULL;
  protocol_rtmp_instance *pIns = (protocol_rtmp_instance*)
          malloc(sizeof(protocol_rtmp_instance));
  if (!pIns)
    return NULL;
  pIns->srs_rtmp = srs_rtmp;
  return (protocol_rtmp_t)pIns;
}

protocol_rtmp_t protocol_rtmp_create2(const char* url) {
  srs_rtmp_t srs_rtmp = srs_rtmp_create2(url);
  if (!srs_rtmp)
    return NULL;
  protocol_rtmp_instance *pIns = (protocol_rtmp_instance*)
          malloc(sizeof(protocol_rtmp_instance));
  if (!pIns)
    return NULL;
  pIns->srs_rtmp = srs_rtmp;
  return (protocol_rtmp_t)pIns;

}
void protocol_rtmp_destroy(protocol_rtmp_t rtmp) {
  if (!rtmp)
    return;
  protocol_rtmp_instance *pIns = (protocol_rtmp_instance*)rtmp;
  srs_rtmp_destroy(pIns->srs_rtmp);
  free(pIns);
}

int32_t protocol_rtmp_handshake(protocol_rtmp_t rtmp) {
  if (!rtmp)
    return -1;
  protocol_rtmp_instance *pIns = (protocol_rtmp_instance*)rtmp;
  return srs_rtmp_handshake(pIns->srs_rtmp);
}
int32_t protocol_rtmp_dns_resolve(protocol_rtmp_t rtmp) {
  if (!rtmp)
    return -1;
  protocol_rtmp_instance *pIns = (protocol_rtmp_instance*)rtmp;
  return srs_rtmp_dns_resolve(pIns->srs_rtmp);
}
int32_t protocol_rtmp_connect_server(protocol_rtmp_t rtmp) {
  if (!rtmp)
    return -1;
  protocol_rtmp_instance *pIns = (protocol_rtmp_instance*)rtmp;
  return srs_rtmp_connect_server(pIns->srs_rtmp);
}
int32_t protocol_rtmp_do_simple_handshake(protocol_rtmp_t rtmp) {
  if (!rtmp)
    return -1;
  protocol_rtmp_instance *pIns = (protocol_rtmp_instance*)rtmp;
  return srs_rtmp_do_simple_handshake(pIns->srs_rtmp);
}
int32_t protocol_rtmp_do_complex_handshake(protocol_rtmp_t rtmp) {
  if (!rtmp)
    return -1;
  protocol_rtmp_instance *pIns = (protocol_rtmp_instance*)rtmp;
  return srs_rtmp_do_complex_handshake(pIns->srs_rtmp);
}
int32_t protocol_rtmp_set_connect_args(protocol_rtmp_t rtmp,
    const char* tcUrl, const char* swfUrl,
    const char* pageUrl, protocol_amf0_t args) {
  if (!rtmp)
    return -1;
  protocol_rtmp_instance *pIns = (protocol_rtmp_instance*)rtmp;
  return srs_rtmp_set_connect_args(pIns->srs_rtmp, tcUrl, swfUrl, pageUrl, args);
}

int32_t protocol_rtmp_connect_app(protocol_rtmp_t rtmp) {
  if (!rtmp)
    return -1;
  protocol_rtmp_instance *pIns = (protocol_rtmp_instance*)rtmp;
  return srs_rtmp_connect_app(pIns->srs_rtmp);
}
#ifdef __cplusplus
}
#endif
