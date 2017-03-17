#include <os_log.h>
#include <protocol_rtmp.h>

int32_t main(int32_t argc, char *argv[]) {
  logi("publish raw h.264 as rtmp stream to server like FMLE/FFMPEG/Encoder\n");
  logi("SRS(ossrs) client librtmp library.\n");
  logi("version: %d.%d.%d\n", protocol_rtmp_version_major(),
      protocol_rtmp_version_minor(),
      protocol_rtmp_version_revision());

  if (argc <= 2) {
    logi("Usage: %s <h264_raw_file> <rtmp_publish_url>\n", argv[0]);
    logi("     h264_raw_file: the h264 raw steam file.\n");
    logi("     rtmp_publish_url: the rtmp publish url.\n");
    logi("For example:\n");
    logi("     %s ./720p.h264.raw rtmp://127.0.0.1:1935/live/livestream\n", argv[0]);
    logi("Where the file: http://winlinvip.github.io/srs.release/3rdparty/720p.h264.raw\n");
    logi("See: https://github.com/ossrs/srs/issues/66\n");
    exit(-1);
  }

  const char* rtmp_url = argv[2];
  // connect rtmp context
  protocol_rtmp_t rtmp = protocol_rtmp_create(rtmp_url);

  if (protocol_rtmp_handshake(rtmp) != 0) {
    loge("simple handshake failed.");
    goto rtmp_destroy;
  }
  logi("simple handshake success");

  if (protocol_rtmp_connect_app(rtmp) != 0) {
    loge("connect vhost/app failed.");
    goto rtmp_destroy;
  }
  logi("connect vhost/app success");

rtmp_destroy:
  protocol_rtmp_destroy(rtmp);
  return 0;
}
