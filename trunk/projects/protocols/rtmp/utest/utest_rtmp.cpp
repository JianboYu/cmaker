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

  if (argc <= 3) {
    logi("Usage: %s <h264_raw_file> <h264_raw_len_file> <rtmp_publish_url>\n", argv[0]);
    logi("     h264_raw_file: the h264 raw steam file.\n");
    logi("     rtmp_publish_url: the rtmp publish url.\n");
    logi("For example:\n");
    logi("     %s ./720p.h264.raw rtmp://127.0.0.1:1935/live/livestream\n", argv[0]);
    logi("Where the file: http://winlinvip.github.io/srs.release/3rdparty/720p.h264.raw\n");
    logi("See: https://github.com/ossrs/srs/issues/66\n");
    exit(-1);
  }

  const char* rtmp_h264_file = argv[1];
  const char* rtmp_h264_len_file = argv[2];
  const char* rtmp_url = argv[3];

  FILE *fp_stream = NULL;
  FILE *fp_stream_len = NULL;

  fp_stream = fopen(rtmp_h264_file, "r");
  fp_stream_len = fopen(rtmp_h264_len_file, "r");

  scoped_ptr<uint8_t> stream_buffer(new uint8_t[1*1024*1024]);
  int32_t pts = 1000;
  int32_t dts = 1000;

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

  while (1) {
    int32_t len = 0;
    int32_t readed = fscanf(fp_stream_len, "%d\n", &len);
    if (readed < 0 || len <= 0) {
      rewind(fp_stream);
      rewind(fp_stream_len);
      readed = fscanf(fp_stream_len, "%d\n", &len);
      if (readed < 0 || len <= 0) {
        CHECK(!"BUG");
      }
    }
    readed = fread(stream_buffer.get(), 1, len, fp_stream);
    CHECK_EQ(readed, len);

    dts += 1000/30;
    pts = dts;
    // send out the h264 packet over RTMP
    int ret = protocol_h264_write_raw_frames(rtmp, stream_buffer.get(), len, dts, pts);

    //ERROR_H264_DROP_BEFORE_SPS_PPS      3043
    //ERROR_H264_DUPLICATED_SPS           3044
    //ERROR_H264_DUPLICATED_PPS           3045
    if (ret == 0) {
    } else if (ret != 0 && (ret == 3043 || ret == 3044 || ret == 3045)) {
      logi("ignore duplicated sps/pps\n");
    } else {
      loge("send h264 raw data failed. ret=%d\n", ret);
      break;
    }
    logi("send h264 raw data sucess size: %d\n", len);

    os_msleep(33);
  }
rtmp_destroy:
  protocol_rtmp_destroy(rtmp);
  if (fp_stream)
    fclose(fp_stream);
  if (fp_stream_len)
    fclose(fp_stream_len);
  return 0;
}
