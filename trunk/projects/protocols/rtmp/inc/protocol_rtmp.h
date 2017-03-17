#ifndef _PROTOCOL_RTMP_H_
#define _PROTOCOL_RTMP_H_

#ifdef __cplusplus
extern "C" {
#endif

int protocol_rtmp_version_major();
int protocol_rtmp_version_minor();
int protocol_rtmp_version_revision();

typedef void* protocol_rtmp_t;
typedef void* protocol_amf0_t;

/**
* create/destroy a rtmp protocol stack.
* @url rtmp url, for example:
*         rtmp://localhost/live/livestream
*
* @return a rtmp handler, or NULL if error occured.
*/
protocol_rtmp_t protocol_rtmp_create(const char* url);
/**
* create rtmp with url, used for connection specified application.
* @param url the tcUrl, for exmple:
*         rtmp://localhost/live
* @remark this is used to create application connection-oriented,
*       for example, the bandwidth client used this, no stream specified.
*
* @return a rtmp handler, or NULL if error occured.
*/
protocol_rtmp_t protocol_rtmp_create2(const char* url);
/**
* close and destroy the rtmp stack.
* @remark, user should never use the rtmp again.
*/
void protocol_rtmp_destroy(protocol_rtmp_t rtmp);

/*************************************************************
**************************************************************
* RTMP protocol stack
**************************************************************
*************************************************************/
/**
* connect and handshake with server
* category: publish/play
* previous: rtmp-create
* next: connect-app
*
* @return 0, success; otherswise, failed.
*/
/**
* simple handshake specifies in rtmp 1.0,
* not depends on ssl.
*/
/**
* protocol_rtmp_handshake equals to invoke:
*       protocol_rtmp_dns_resolve()
*       protocol_rtmp_connect_server()
*       protocol_rtmp_do_simple_handshake()
* user can use these functions if needed.
*/
int protocol_rtmp_handshake(protocol_rtmp_t rtmp);
// parse uri, create socket, resolve host
int protocol_rtmp_dns_resolve(protocol_rtmp_t rtmp);
// connect socket to server
int protocol_rtmp_connect_server(protocol_rtmp_t rtmp);
// do simple handshake over socket.
int protocol_rtmp_do_simple_handshake(protocol_rtmp_t rtmp);
// do complex handshake over socket.
int protocol_rtmp_do_complex_handshake(protocol_rtmp_t rtmp);

/**
* set the args of connect packet for rtmp.
* @param args, the extra amf0 object args.
* @remark, all params can be NULL to ignore.
* @remark, user should never free the args for we directly use it.
*/
int protocol_rtmp_set_connect_args(protocol_rtmp_t rtmp,
    const char* tcUrl, const char* swfUrl, const char* pageUrl, protocol_amf0_t args
);

/**
* connect to rtmp vhost/app
* category: publish/play
* previous: handshake
* next: publish or play
*
* @return 0, success; otherswise, failed.
*/
int protocol_rtmp_connect_app(protocol_rtmp_t rtmp);

/**
* connect to server, get the debug srs info.
* 
* SRS debug info:
* @param srs_server_ip, 128bytes, debug info, server ip client connected at.
* @param srs_server, 128bytes, server info.
* @param srs_primary, 128bytes, primary authors.
* @param srs_authors, 128bytes, authors.
* @param srs_version, 32bytes, server version.
* @param srs_id, int, debug info, client id in server log.
* @param srs_pid, int, debug info, server pid in log.
*
* @return 0, success; otherswise, failed.
*/
int protocol_rtmp_connect_app2(protocol_rtmp_t rtmp,
    char server_ip[128], char server[128],
    char primary[128], char authors[128],
    char version[32], int* id, int* pid
);

/**
* play a live/vod stream.
* category: play
* previous: connect-app
* next: destroy
* @return 0, success; otherwise, failed.
*/
int protocol_rtmp_play_stream(protocol_rtmp_t rtmp);

/**
* publish a live stream.
* category: publish
* previous: connect-app
* next: destroy
* @return 0, success; otherwise, failed.
*/
int protocol_rtmp_publish_stream(protocol_rtmp_t rtmp);

/**
* do bandwidth check with srs server.
* 
* bandwidth info:
* @param start_time, output the start time, in ms.
* @param end_time, output the end time, in ms.
* @param play_kbps, output the play/download kbps.
* @param publish_kbps, output the publish/upload kbps.
* @param play_bytes, output the play/download bytes.
* @param publish_bytes, output the publish/upload bytes.
* @param play_duration, output the play/download test duration, in ms.
* @param publish_duration, output the publish/upload test duration, in ms.
*
* @return 0, success; otherswise, failed.
*/
int protocol_rtmp_bandwidth_check(protocol_rtmp_t rtmp,
    int64_t* start_time, int64_t* end_time,
    int* play_kbps, int* publish_kbps,
    int* play_bytes, int* publish_bytes,
    int* play_duration, int* publish_duration
);

#ifdef __cplusplus
}
#endif

#endif //_PROTOCOL_RTMP_H_
