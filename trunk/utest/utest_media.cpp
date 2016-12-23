#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG "utest_media"

#include <stdio.h>
#include <string.h>
#include <os_typedefs.h>
#include <os_assert.h>
#include <os_log.h>
#include <os_time.h>
#include <os_thread.h>
#include <core_scoped_ptr.h>
#include <utility_circle_queue.h>
#include <utility_buffer_queue.h>
#include <utility_memory.h>
#include <protocol_rtp_sender.h>
#include <protocol_rtp_receiver.h>
#include <protocol_rtp_payload_registry.h>
#include <protocol_rtp_header_parser.h>

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Index.h>
#include <OMX_Video.h>
#include <OMX_AudioExt.h>
#include <OMX_IndexExt.h>

using namespace os;
using namespace core;
using namespace utility;
using namespace protocol;

#define kTestSsrc (1243)
const int kNumFrames = 30;
const int kPayloadType = 123;
const int kRtxPayloadType = 98;

void DumpRTPHeader(uint8_t *rtp_header) {
  log_verbose("tag", "V=%d\n", (*rtp_header & 0xc0) >> 6);
  log_verbose("tag", "P=%d\n", (*rtp_header & 0x20) >> 5);
  log_verbose("tag", "S=%d\n", (*rtp_header & 0x10) >> 4);
  log_verbose("tag", "CC=%d\n", (*rtp_header & 0x0f));
  log_verbose("tag", "M=%d\n", (*(rtp_header+1) & 0x80) >> 7);
  log_verbose("tag", "PT=%d\n", (*(rtp_header+1) & 0x7f));
  log_verbose("tag", "SEQ=%d\n", *((uint16_t*)rtp_header+1));
  log_verbose("tag", "TS=%u\n", *((uint32_t*)rtp_header+1));
  log_verbose("tag", "SSRC=%u\n", *((uint32_t*)rtp_header+2));
}

class VerifyingRtxReceiver : public NullRtpData {
 public:
  VerifyingRtxReceiver() {}

  int32_t OnReceivedPayloadData(
      const uint8_t* data,
      size_t size,
      const protocol::WebRtcRTPHeader* rtp_header) override {
    if (!_sequence_numbers.empty())
      CHECK_EQ(kTestSsrc, (int32_t)rtp_header->header.ssrc);
    _sequence_numbers.push_back(rtp_header->header.sequenceNumber);
    log_verbose("tag", "Recevied payload data addr: %p size: %d\n", data, size);
    return 0;
  }
  std::list<uint16_t> _sequence_numbers;
};

class TestRtpFeedback : public NullRtpFeedback {
public:
  explicit TestRtpFeedback() {}
  virtual ~TestRtpFeedback() {}

  void OnIncomingSSRCChanged(const uint32_t ssrc) override {
    logv("OnIncomingSSRCChanged..\n");
    //rtp_rtcp_->SetRemoteSSRC(ssrc);
  }

 private:
};

class MockTransport : public Transport {
public:
  explicit MockTransport(uint32_t rtx_ssrc)
      : _count(0),
        _packet_loss(0),
        _consecutive_drop_start(0),
        _consecutive_drop_end(0),
        _rtx_ssrc(rtx_ssrc),
        _count_rtx_ssrc(0),
        _rtp_payload_registry(NULL),
        _rtp_receiver(NULL),
        _rtp_sender(NULL) {}

  virtual bool SendRtp(const uint8_t* packet,
                       size_t length,
                       const PacketOptions& options) {
    logv("SendRtp idx: %08d addr: %p size: %d\n", _count, packet, length);
    _count++;
    const unsigned char* ptr = static_cast<const unsigned char*>(packet);
    //DumpRTPHeader((uint8_t*)ptr);
    uint32_t ssrc = (ptr[8] << 24) + (ptr[9] << 16) + (ptr[10] << 8) + ptr[11];
    if (ssrc == _rtx_ssrc)
      _count_rtx_ssrc++;
    uint16_t sequence_number = (ptr[2] << 8) + ptr[3];
    size_t packet_length = length;
    uint8_t restored_packet[1500];
    RTPHeader header;
    std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
    if (!parser->Parse(ptr, length, &header)) {
      return false;
    }
    if (!_rtp_payload_registry->IsRtx(header)) {
      // Don't store retransmitted packets since we compare it to the list
      // created by the receiver.
      _expected_sequence_numbers.insert(_expected_sequence_numbers.end(),
                                        sequence_number);
    }
    if (_packet_loss > 0) {
      if ((_count % _packet_loss) == 0) {
        return true;
      }
    } else if (_count >= _consecutive_drop_start &&
               _count < _consecutive_drop_end) {
      return true;
    }
    if (_rtp_payload_registry->IsRtx(header)) {
      // Remove the RTX header and parse the original RTP header.
      /*CHECK_EQ(true, _rtp_payload_registry->RestoreOriginalPacket(
          restored_packet, ptr, &packet_length, _rtp_receiver->SSRC(), header));*/
      if (!parser->Parse(restored_packet, packet_length, &header)) {
        return false;
      }
      ptr = restored_packet;
    } else {
      _rtp_payload_registry->SetIncomingPayloadType(header);
    }

    PayloadUnion payload_specific;
    if (!_rtp_payload_registry->GetPayloadSpecifics(header.payloadType,
                                                    &payload_specific)) {
      return false;
    }
    if (!_rtp_receiver->IncomingRtpPacket(header, ptr + header.headerLength,
                                          packet_length - header.headerLength,
                                          payload_specific, true)) {
      return false;
    }
   return true;
  }
  virtual bool SendRtcp(const uint8_t* packet, size_t length) {
    return 0;//_rtp_sender->IncomingRtcpPacket(packet, length) == 0;
  }
  void SetSendModule(RTPSender* sender,
                    RTPPayloadRegistry* rtp_payload_registry,
                    RtpReceiver* receiver) {
    _rtp_sender = sender;
    _rtp_payload_registry = rtp_payload_registry;
    _rtp_receiver = receiver;
  }
  void DropEveryNthPacket(int n) { _packet_loss = n; }
  void DropConsecutivePackets(int start, int total) {
    _consecutive_drop_start = start;
    _consecutive_drop_end = start + total;
    _packet_loss = 0;
  }
private:
  int32_t _count;
  int32_t _packet_loss;
  int32_t _consecutive_drop_start;
  int32_t _consecutive_drop_end;
  uint32_t _rtx_ssrc;
  int32_t _count_rtx_ssrc;
  RTPPayloadRegistry* _rtp_payload_registry;
  RtpReceiver* _rtp_receiver;
  RTPSender* _rtp_sender;
  std::set<uint16_t> _expected_sequence_numbers;
};


template<class T>
static void InitOMXParams(T *params) {
  memset(params, 0, sizeof(T));
  params->nSize = sizeof(T);
  params->nVersion.s.nVersionMajor = 1;
  params->nVersion.s.nVersionMinor = 0;
  params->nVersion.s.nRevision = 0;
  params->nVersion.s.nStep = 0;
}

typedef struct OMXContext {
  OMX_HANDLETYPE hComponent;
  OMX_BUFFERHEADERTYPE *inBuffer[8];
  OMX_BUFFERHEADERTYPE *outBuffer[8];
  cirq_handle fbd;
  cirq_handle ebd;
  os::Thread *thread;
  OMX_TICKS  ts;
  bool encode;
  bool video;
  FILE *fp;
  FILE *fp_encoded;
  FILE *fp_len;
  RTPSender *rtp_sender;
}OMXContext;

OMX_ERRORTYPE sEventHandler(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_IN OMX_PTR pAppData,
  OMX_IN OMX_EVENTTYPE eEvent,
  OMX_IN OMX_U32 nData1,
  OMX_IN OMX_U32 nData2,
  OMX_IN OMX_PTR pEventData) {
    logv("EventHandler com: %p appdata: %p\n", hComponent, pAppData);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE sEmptyBufferDone(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_IN OMX_PTR pAppData,
  OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) {
    logv("EmptyBufferDone com: %p appdata: %p\n", hComponent, pAppData);
    OMXContext *omx_ctx = (OMXContext *)pAppData;
    int32_t status = cirq_enqueue(omx_ctx->ebd, pBuffer);
    CHECK_EQ(0, status);

    return OMX_ErrorNone;
}
OMX_ERRORTYPE sFillBufferDone(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_IN OMX_PTR pAppData,
  OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) {
    logv("FillBufferDone com: %p appdata: %p\n", hComponent, pAppData);
    OMXContext *omx_ctx = (OMXContext *)pAppData;
    int32_t status = cirq_enqueue(omx_ctx->fbd, pBuffer);
    CHECK_EQ(0, status);
    return OMX_ErrorNone;
}

bool thread_loop(void *ctx) {
  OMX_ERRORTYPE oRet = OMX_ErrorNone;
  OMXContext *omx_ctx = (OMXContext *)ctx;

  OMX_HANDLETYPE hComponent = omx_ctx->hComponent;
  void *pdata = NULL;
  cirq_dequeue(omx_ctx->ebd, &pdata);
  OMX_BUFFERHEADERTYPE* pBuffer = (OMX_BUFFERHEADERTYPE*)pdata;
  if (pBuffer) {
    pBuffer->nFlags = 0;
    pBuffer->nFilledLen = 1280*720*3 >> 1;
    pBuffer->nOffset = 0;
    pBuffer->nTimeStamp = omx_ctx->ts;

    uint32_t readed = fread(pBuffer->pBuffer, 1, pBuffer->nFilledLen, omx_ctx->fp);
    if (readed < pBuffer->nFilledLen) {
      rewind(omx_ctx->fp);
      readed = fread(pBuffer->pBuffer, 1, pBuffer->nFilledLen, omx_ctx->fp);
      CHECK_EQ(readed, pBuffer->nFilledLen);
    }
    oRet = OMX_EmptyThisBuffer(hComponent, pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
    logi("EmptyThisBuffer: %p\n", pBuffer);
    omx_ctx->ts += 3600;
  }

  pdata = NULL;
  cirq_dequeue(omx_ctx->fbd, &pdata);
  pBuffer = (OMX_BUFFERHEADERTYPE*)pdata;
  if (pBuffer) {
    if (pBuffer->nFilledLen > 0) {
      RTPFragmentationHeader fragment;
      fragment.VerifyAndAllocateFragmentationHeader(1);
      fragment.fragmentationLength[0] = pBuffer->nFilledLen;
      fragment.fragmentationOffset[0] = 0;
      fragment.fragmentationTimeDiff[0] = 0;
      fragment.fragmentationPlType[0] = 0;

      int32_t ret = omx_ctx->rtp_sender->SendOutgoingData(
                     kVideoFrameDelta, kPayloadType, pBuffer->nTimeStamp,
                     pBuffer->nTimeStamp / 90, pBuffer->pBuffer, pBuffer->nFilledLen, &fragment, NULL);
      CHECK_EQ(0, ret);
    }
    pBuffer->nFlags = 0;
    pBuffer->nFilledLen = 0;
    pBuffer->nOffset = 0;
    pBuffer->nTimeStamp = 0;
    oRet = OMX_FillThisBuffer(hComponent, pBuffer);
    logi("FillThisBuffer: %p\n", pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }
  os_msleep(40);
  return true;
}

void DumpPortDefine(OMX_PARAM_PORTDEFINITIONTYPE *def) {
    logv("-----------------Port Define---------------\n");
    logv("Index: %d\n", def->nPortIndex);
    logv("Dir: %s\n", def->eDir == OMX_DirInput ? "DirInput" : "DirOuput");
    logv("BufMinCount: %d\n", def->nBufferCountMin);
    logv("BufActCount: %d\n", def->nBufferCountActual);
    logv("Domain: %s\n", def->eDomain == OMX_PortDomainVideo ? "Video" : "Audio");
    if (def->eDomain == OMX_PortDomainVideo) {
      logv("Width: %d\n", def->format.video.nFrameWidth);
      logv("Height: %d\n", def->format.video.nFrameHeight);
      logv("Stride: %d\n", def->format.video.nStride);
      logv("SliceH: %d\n", def->format.video.nSliceHeight);
      logv("Bitrate: %d\n", def->format.video.nBitrate);
      logv("Framerate: %d\n", def->format.video.xFramerate >> 16);
      logv("BufAlign: %d\n", def->nBufferAlignment);
      logv("CompressFmt: %x\n", def->format.video.eCompressionFormat);
      logv("ColorFmt: %x\n", def->format.video.eColorFormat);
    }
}

int32_t main(int argc, char *argv[]) {
  log_setlevel(eLogInfo);
  OMX_ERRORTYPE oRet = OMX_ErrorNone;
  oRet = OMX_Init();
  CHECK_EQ(oRet, OMX_ErrorNone);
  scoped_array<char> comp_name(new char [128]);
  for (int32_t i = 0; ; ++i) {
    oRet = OMX_ComponentNameEnum((OMX_STRING)comp_name.get(), 128, i);
    if (OMX_ErrorNoMore == oRet)
      break;
    CHECK_EQ(oRet, OMX_ErrorNone);

    log_verbose("tag", "Component[%d]: %s \n", i + 1, comp_name.get());
  }
  OMXContext *omx_ctx = (OMXContext*)malloc(sizeof(OMXContext));
  CHECK(omx_ctx);
  int32_t status = -1;
  int32_t input_buffer_num = 2;
  int32_t output_buffer_num = 2;
  status = cirq_create(&omx_ctx->fbd, input_buffer_num);
  CHECK_EQ(0, status);
  status = cirq_create(&omx_ctx->ebd, output_buffer_num);
  CHECK_EQ(0, status);
  omx_ctx->thread = os::Thread::Create(thread_loop, omx_ctx);
  omx_ctx->ts = 3600;
  omx_ctx->encode = true;
  omx_ctx->fp = fopen("i420_1280x720_8pi_KristenAndSara_60.yuv", "r");
  omx_ctx->fp_encoded = fopen("i420_1280x720_8pi_KristenAndSara_60.h264", "w+");
  omx_ctx->fp_len = fopen("i420_1280x720_8pi_KristenAndSara_60.len", "w+");

  OMX_CALLBACKTYPE omx_cb;
  omx_cb.EventHandler = sEventHandler;
  omx_cb.FillBufferDone = sFillBufferDone;
  omx_cb.EmptyBufferDone = sEmptyBufferDone;
  OMX_HANDLETYPE pHandle;
  oRet = OMX_GetHandle(&pHandle,
    (OMX_STRING)"OMX.omxil.h264.encoder",
    (OMX_PTR)omx_ctx,
    &omx_cb);
  CHECK_EQ(oRet, OMX_ErrorNone);
  omx_ctx->hComponent = pHandle;

  OMX_PARAM_PORTDEFINITIONTYPE def;
  InitOMXParams(&def);
  def.nPortIndex = 0;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);

  def.format.video.nFrameWidth = 1280;
  def.format.video.nFrameHeight = 720;
  def.format.video.nStride = def.format.video.nFrameWidth;
  def.format.video.nSliceHeight = def.format.video.nFrameHeight;
  def.format.video.nBitrate = 0;
  def.format.video.xFramerate = 30 << 16;
  def.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
  def.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
  DumpPortDefine(&def);
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);

  def.nPortIndex = 1;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);
  def.format.video.nFrameWidth = 1280;
  def.format.video.nFrameHeight = 720;
  def.format.video.nStride = def.format.video.nFrameWidth;
  def.format.video.nSliceHeight = def.format.video.nFrameHeight;
  def.format.video.nBitrate = 1000;
  def.format.video.xFramerate = 0;
  def.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  def.format.video.eColorFormat = OMX_COLOR_FormatUnused;
  DumpPortDefine(&def);
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < input_buffer_num; ++i) {
    oRet = OMX_AllocateBuffer(pHandle, &omx_ctx->inBuffer[i], 0, NULL, 1280*720*3 >> 1);
    CHECK_EQ(oRet, OMX_ErrorNone);
    CHECK(omx_ctx->inBuffer[i]);
    logv("Port: %d index: %d Allocate Buffer: %p len: %d\n",
        0, i, omx_ctx->inBuffer[i]->pBuffer, omx_ctx->inBuffer[i]->nFilledLen);
  }

  for (int32_t i = 0; i < output_buffer_num; ++i) {
    oRet = OMX_AllocateBuffer(pHandle, &omx_ctx->outBuffer[i], 1, NULL, 1280*720*3 >> 1);
    CHECK_EQ(oRet, OMX_ErrorNone);
    CHECK(omx_ctx->outBuffer[i]);
    logv("Port: %d index: %d Allocate Buffer: %p len: %d\n",
        1, i, omx_ctx->outBuffer[i]->pBuffer, omx_ctx->outBuffer[i]->nFilledLen);
  }
  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < input_buffer_num; ++i) {
    int32_t status = cirq_enqueue(omx_ctx->ebd, omx_ctx->inBuffer[i]);
    CHECK_EQ(0, status);
    status = cirq_enqueue(omx_ctx->fbd, omx_ctx->outBuffer[i]);
    CHECK_EQ(0, status);
  }

  int32_t ret = -1;
  bool audio __attribute((__unused__)) = false;
  Clock* clock __attribute((__unused__)) = NULL;
  clock = Clock::GetRealTimeClock();
  RtpPacketSender* paced_sender __attribute((__unused__)) = NULL;
  TransportSequenceNumberAllocator* sequence_number_allocator = NULL;
  TransportFeedbackObserver* transport_feedback_callback = NULL;
  BitrateStatisticsObserver* bitrate_callback = NULL;
  FrameCountObserver* frame_count_observer = NULL;
  SendSideDelayObserver* send_side_delay_observer = NULL;
  RtcEventLog* event_log = NULL;
  SendPacketObserver* send_packet_observer = NULL;
  RateLimiter* nack_rate_limiter = NULL;

  scoped_ptr<MockTransport> transport(new MockTransport(1243));
  scoped_ptr<RTPSender> rtp_sender(
                          new RTPSender(audio,
                          clock,
                          transport.get(),
                          paced_sender,
                          sequence_number_allocator,
                          transport_feedback_callback,
                          bitrate_callback,
                          frame_count_observer,
                          send_side_delay_observer,
                          event_log,
                          send_packet_observer,
                          nack_rate_limiter));
  rtp_sender->SetStartTimestamp(123654, true);
  log_verbose("tag", "start tick: %lld ts: %u\n", clock->TimeInMilliseconds(),
                      rtp_sender->StartTimestamp());
  scoped_array<uint8_t> rtp_header(new uint8_t[128]);
  ret = rtp_sender->BuildRTPheader(rtp_header.get(),
                              126,
                              true,
                              1357,
                              2468);
  CHECK_GT(ret, 0);
  DumpRTPHeader(rtp_header.get());

  scoped_ptr<RTPPayloadRegistry> rtp_payload_registry(new RTPPayloadRegistry(
                                RTPPayloadStrategy::CreateStrategy(false)));
  scoped_ptr<VerifyingRtxReceiver> receiver(new VerifyingRtxReceiver());
  scoped_ptr<TestRtpFeedback> rtp_feedback(new TestRtpFeedback());
  scoped_ptr<RtpReceiver> rtp_receiver(RtpReceiver::CreateVideoReceiver(
    clock, receiver.get(), rtp_feedback.get(), rtp_payload_registry.get()));

  transport->SetSendModule(NULL,
                    rtp_payload_registry.get(),
                    rtp_receiver.get());

  rtp_sender->SetSSRC(kTestSsrc);
  ret = rtp_sender->RegisterPayload("H264", kPayloadType, 9000, 1, 0);
  CHECK_EQ(0, ret);
  scoped_array<uint8_t> payload_data(new uint8_t[65000]);
  uint32_t payload_data_length = 3030;
  for (uint32_t i = 0; i < payload_data_length; ++i) {
    payload_data[i] = i % 128;
  }
  rtp_payload_registry->SetRtxSsrc(kTestSsrc + 1);
  bool created_new_payload = false;
  ret = rtp_payload_registry->RegisterReceivePayload(
                      "H264",
                      kPayloadType,
                      9000,
                      1,
                      0, &created_new_payload);
  CHECK_EQ(0, ret);
  omx_ctx->rtp_sender = rtp_sender.get();

  uint32_t tid = 0;
  omx_ctx->thread->start(tid);
  os_sleep(1000);

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < input_buffer_num; ++i) {
    oRet = OMX_FreeBuffer(pHandle, 0, omx_ctx->inBuffer[i]);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }
  for (int32_t i = 0; i < output_buffer_num; ++i) {
    oRet = OMX_FreeBuffer(pHandle, 1, omx_ctx->outBuffer[i]);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);


  oRet = OMX_FreeHandle(pHandle);
  CHECK_EQ(oRet, OMX_ErrorNone);

  oRet = OMX_Deinit();
  CHECK_EQ(oRet, OMX_ErrorNone);

  omx_ctx->thread->stop();
  delete omx_ctx->thread;
  status = cirq_destory(omx_ctx->fbd);
  CHECK_EQ(0, status);
  status = cirq_destory(omx_ctx->ebd);
  CHECK_EQ(0, status);
  fclose(omx_ctx->fp);
  fclose(omx_ctx->fp_encoded);

  free(omx_ctx);
  return 0;
}
