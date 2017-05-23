#ifndef _PROTOCOL_RTMP_H_
#define _PROTOCOL_RTMP_H_

#include <os_typedefs.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t protocol_rtmp_version_major();
int32_t protocol_rtmp_version_minor();
int32_t protocol_rtmp_version_revision();

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
int32_t protocol_rtmp_handshake(protocol_rtmp_t rtmp);
// parse uri, create socket, resolve host
int32_t protocol_rtmp_dns_resolve(protocol_rtmp_t rtmp);
// connect socket to server
int32_t protocol_rtmp_connect_server(protocol_rtmp_t rtmp);
// do simple handshake over socket.
int32_t protocol_rtmp_do_simple_handshake(protocol_rtmp_t rtmp);
// do complex handshake over socket.
int32_t protocol_rtmp_do_complex_handshake(protocol_rtmp_t rtmp);

/**
* set the args of connect packet for rtmp.
* @param args, the extra amf0 object args.
* @remark, all params can be NULL to ignore.
* @remark, user should never free the args for we directly use it.
*/
int32_t protocol_rtmp_set_connect_args(protocol_rtmp_t rtmp,
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
int32_t protocol_rtmp_connect_app(protocol_rtmp_t rtmp);

/**
* connect to server, get the debug srs info.
* 
* SRS debug info:
* @param srs_server_ip, 128bytes, debug info, server ip client connected at.
* @param srs_server, 128bytes, server info.
* @param srs_primary, 128bytes, primary authors.
* @param srs_authors, 128bytes, authors.
* @param srs_version, 32bytes, server version.
* @param srs_id, int32_t, debug info, client id in server log.
* @param srs_pid, int32_t, debug info, server pid in log.
*
* @return 0, success; otherswise, failed.
*/
int32_t protocol_rtmp_connect_app2(protocol_rtmp_t rtmp,
    char server_ip[128], char server[128],
    char primary[128], char authors[128],
    char version[32], int32_t* id, int32_t* pid
);

/**
* play a live/vod stream.
* category: play
* previous: connect-app
* next: destroy
* @return 0, success; otherwise, failed.
*/
int32_t protocol_rtmp_play_stream(protocol_rtmp_t rtmp);

/**
* publish a live stream.
* category: publish
* previous: connect-app
* next: destroy
* @return 0, success; otherwise, failed.
*/
int32_t protocol_rtmp_publish_stream(protocol_rtmp_t rtmp);

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
int32_t protocol_rtmp_bandwidth_check(protocol_rtmp_t rtmp,
    int64_t* start_time, int64_t* end_time,
    int32_t* play_kbps, int32_t* publish_kbps,
    int32_t* play_bytes, int32_t* publish_bytes,
    int32_t* play_duration, int32_t* publish_duration
);

/*************************************************************
**************************************************************
* h264 raw codec
**************************************************************
*************************************************************/
/**
* write h.264 raw frame over RTMP to rtmp server.
* @param frames the input h264 raw data, encoded h.264 I/P/B frames data.
*       frames can be one or more than one frame,
*       each frame prefixed h.264 annexb header, by N[00] 00 00 01, where N>=0,
*       for instance, frame = header(00 00 00 01) + payload(67 42 80 29 95 A0 14 01 6E 40)
*       about annexb, @see H.264-AVC-ISO_IEC_14496-10.pdf, page 211.
* @param frames_size the size of h264 raw data.
*       assert frames_size > 0, at least has 1 bytes header.
* @param dts the dts of h.264 raw data.
* @param pts the pts of h.264 raw data.
*
* @remark, user should free the frames.
* @remark, the tbn of dts/pts is 1/1000 for RTMP, that is, in ms.
* @remark, cts = pts - dts
* @remark, use srs_h264_startswith_annexb to check whether frame is annexb format.
* @example /trunk/research/librtmp/srs_h264_raw_publish.c
* @see https://github.com/ossrs/srs/issues/66
*
* @return 0, success; otherswise, failed.
*       for dvbsp error, @see srs_h264_is_dvbsp_error().
*       for duplictated sps error, @see srs_h264_is_duplicated_sps_error().
*       for duplictated pps error, @see srs_h264_is_duplicated_pps_error().
*/
/**
For the example file:
    http://winlinvip.github.io/srs.release/3rdparty/720p.h264.raw
The data sequence is:
    // SPS
    000000016742802995A014016E40
    // PPS
    0000000168CE3880
    // IFrame
    0000000165B8041014C038008B0D0D3A071.....
    // PFrame
    0000000141E02041F8CDDC562BBDEFAD2F.....
User can send the SPS+PPS, then each frame:
    // SPS+PPS
    srs_h264_write_raw_frames('000000016742802995A014016E400000000168CE3880', size, dts, pts)
    // IFrame
    srs_h264_write_raw_frames('0000000165B8041014C038008B0D0D3A071......', size, dts, pts)
    // PFrame
    srs_h264_write_raw_frames('0000000141E02041F8CDDC562BBDEFAD2F......', size, dts, pts)
User also can send one by one:
    // SPS
    srs_h264_write_raw_frames('000000016742802995A014016E4', size, dts, pts)
    // PPS
    srs_h264_write_raw_frames('00000000168CE3880', size, dts, pts)
    // IFrame
    srs_h264_write_raw_frames('0000000165B8041014C038008B0D0D3A071......', size, dts, pts)
    // PFrame
    srs_h264_write_raw_frames('0000000141E02041F8CDDC562BBDEFAD2F......', size, dts, pts)
*/
int32_t protocol_h264_write_raw_frames(protocol_rtmp_t rtmp,
    uint8_t* frames, int32_t frames_size, uint32_t dts, uint32_t pts);

int32_t protocol_rtmp_write_packet(protocol_rtmp_t rtmp,
    char type, u_int32_t timestamp, char* data, int size
);


#ifdef __cplusplus
}
#endif

#endif //_PROTOCOL_RTMP_H_
