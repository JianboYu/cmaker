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

int32_t protocol_rtmp_connect_app2(protocol_rtmp_t rtmp,
    char server_ip[128], char server[128],
    char primary[128], char authors[128],
    char version[32], int32_t* id, int32_t* pid) {
  if (!rtmp)
    return -1;
  protocol_rtmp_instance *pIns = (protocol_rtmp_instance*)rtmp;
  return srs_rtmp_connect_app2(pIns->srs_rtmp, server_ip, server,
                               primary, authors, version, id, pid);

}

int32_t protocol_rtmp_play_stream(protocol_rtmp_t rtmp) {
  if (!rtmp)
    return -1;
  protocol_rtmp_instance *pIns = (protocol_rtmp_instance*)rtmp;
  return srs_rtmp_play_stream(pIns->srs_rtmp);
}

int32_t protocol_rtmp_publish_stream(protocol_rtmp_t rtmp) {
  if (!rtmp)
    return -1;
  protocol_rtmp_instance *pIns = (protocol_rtmp_instance*)rtmp;
  return srs_rtmp_publish_stream(pIns->srs_rtmp);
}

int32_t protocol_h264_write_raw_frames(srs_rtmp_t rtmp,
    uint8_t* frames, int32_t frames_size, uint32_t dts, uint32_t pts) {
  if (!rtmp)
    return -1;
  protocol_rtmp_instance *pIns = (protocol_rtmp_instance*)rtmp;
  return srs_h264_write_raw_frames(pIns->srs_rtmp, (char*)frames, frames_size, dts, pts);
}

#ifdef __cplusplus
}
#endif