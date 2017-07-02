#include <os_log.h>
#include <os_assert.h>
#include <os_time.h>
#include <core_scoped_ptr.h>
#include <protocol_rtmp.h>

using namespace core;

int32_t main(int32_t argc, char *argv[]) {
  logi("publish raw h.264 as rtmp stream to server like FMLE/FFMPEG/Encoder\n");
  logi("SRS(ossrs) client librtmp library.\n");
  logi("version: %d.%d.%d\n", protocol_rtmp_version_major(),
      protocol_rtmp_version_minor(),
      protocol_rtmp_version_revision());

  if (argc <= 1) {
        printf("Usage: %s <rtmp_url>\n"
            "   rtmp_url     RTMP stream url to play\n"
            "For example:\n"
            "   %s rtmp://127.0.0.1:1935/live/livestream\n",
            argv[0], argv[0]);

    logi("Usage: %s rtmp_url RTMP stream url to play\n", argv[0]);
    logi("For example:\n");
    logi("     %s rtmp://127.0.0.1:1935/live/livestream\n", argv[0]);
    exit(-1);
  }

  const char* rtmp_url = argv[1];
  // connect rtmp context
  protocol_rtmp_t rtmp = protocol_rtmp_create(rtmp_url);

  if (protocol_rtmp_handshake(rtmp) != 0) {
    loge("simple handshake failed.\n");
    goto rtmp_destroy;
  }
  logi("simple handshake success\n");

  if (protocol_rtmp_connect_app(rtmp) != 0) {
    loge("connect vhost/app failed.\n");
    goto rtmp_destroy;
  }
  logi("connect vhost/app success\n");

  if (protocol_rtmp_play_stream(rtmp) != 0) {
    loge("publish stream failed.");
    goto rtmp_destroy;
  }
  logi("play stream success");

  while (1) {
    int32_t size;
    char type;
    char* data;
    u_int32_t timestamp;

    if (protocol_rtmp_read_packet(rtmp, &type, &timestamp, &data, &size) != 0) {
      goto rtmp_destroy;
    }

    if (protocol_human_print_rtmp_packet(type, timestamp, data, size) != 0) {
      goto rtmp_destroy;
    }

    free(data);

  }
rtmp_destroy:
  protocol_rtmp_destroy(rtmp);
  return 0;
}
