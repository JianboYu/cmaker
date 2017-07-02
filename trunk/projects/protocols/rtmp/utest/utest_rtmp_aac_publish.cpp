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
    logi("Usage: %s <adts_aac_file> <adts_aac_len_file> <rtmp_publish_url>\n", argv[0]);
    logi("     adts_aac_file: the aac steam file.\n");
    logi("     adts_aac_len_file: the aac steam len file.\n");
    logi("     rtmp_publish_url: the rtmp publish url.\n");
    logi("For example:\n");
    logi("     %s ./44k-stereo-16width.aac 44k-stereo-16width.aac.len rtmp://127.0.0.1:1935/live/livestream\n", argv[0]);
    exit(-1);
  }

  const char* rtmp_aac_file = argv[1];
  const char* rtmp_aac_len_file = argv[2];
  const char* rtmp_url = argv[3];

  uint32_t timestamp = 0;
  uint32_t time_delta = 23;

  FILE *fp_stream = NULL;
  FILE *fp_stream_len = NULL;

  fp_stream = fopen(rtmp_aac_file, "r");
  fp_stream_len = fopen(rtmp_aac_len_file, "r");

  scoped_ptr<uint8_t> stream_buffer(new uint8_t[1*1024*1024]);

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

  if (protocol_rtmp_publish_stream(rtmp) != 0) {
    loge("publish stream failed.");
    goto rtmp_destroy;
  }
  logi("publish stream success");

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

    // 0 = Linear PCM, platform endian
    // 1 = ADPCM
    // 2 = MP3
    // 7 = G.711 A-law logarithmic PCM
    // 8 = G.711 mu-law logarithmic PCM
    // 10 = AAC
    // 11 = Speex
    char sound_format = 10;
    // 3 = 44.1 kHz
    char sound_rate = 3;
    // 1 = 16-bit samples
    char sound_size = 1;
    // 1 = Stereo sound
    char sound_type = 1;

    timestamp += time_delta;

    int ret = 0;
    if ((ret = protocol_audio_write_raw_frame(rtmp,
        sound_format, sound_rate, sound_size, sound_type,
        (char*)stream_buffer.get(), len, timestamp)) != 0) {
        loge("send audio raw data failed. ret=%d", ret);
        goto rtmp_destroy;
    }
    logi("send aac raw data sucess size: %d ts: %d\n", len, timestamp);

    os_msleep(time_delta);
  }
rtmp_destroy:
  protocol_rtmp_destroy(rtmp);
  if (fp_stream)
    fclose(fp_stream);
  if (fp_stream_len)
    fclose(fp_stream_len);
  return 0;
}
