#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG "utest_media"

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <os_typedefs.h>
#include <os_assert.h>
#include <os_log.h>
#include <os_time.h>
#include <os_thread.h>
#include <os_socket.h>
#include <os_socket_manager.h>

#include <core_scoped_ptr.h>
#include <utility_circle_queue.h>
#include <utility_buffer_queue.h>
#include <utility_memory.h>
#include <protocol_rtp_rtcp.h>
#include <protocol_transport.h>
//#include <protocol_rtp_receiver.h>
//#include <protocol_rtp_payload_registry.h>
//#include <protocol_rtp_header_parser.h>
//#include <protocol_rtcp_sender.h>
//#include <protocol_rtcp_receiver.h>
#include <protocol_receive_statistics.h>

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

#define kTestSsrc (3248)
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
  VerifyingRtxReceiver() {
    _fp = fopen("recv_rtp.h264", "w+");
  }

  int32_t OnReceivedPayloadData(
      const uint8_t* data,
      size_t size,
      const protocol::WebRtcRTPHeader* rtp_header) override {
    if (!_sequence_numbers.empty())
      CHECK_EQ(kTestSsrc, (int32_t)rtp_header->header.ssrc);
    _sequence_numbers.push_back(rtp_header->header.sequenceNumber);
    //uint8_t numCSRCs;
    //uint32_t arrOfCSRCs[kRtpCsrcSize];
    //int payload_type_frequency;
    log_verbose("tag", "RTP header:\n");
    log_verbose("tag", "  markebit: %d\n"
                       "  payload: %d\n"
                       "  seq: %u\n"
                       "  ts: %lu\n"
                       "  ssrc: %08x\n"
                       "  frametype: %d\n"
                       "  head len: %d\n"
                       "  pading len: %d\n",
                rtp_header->header.markerBit,
                rtp_header->header.payloadType,
                rtp_header->header.sequenceNumber,
                rtp_header->header.timestamp,
                rtp_header->header.ssrc,
                rtp_header->frameType,
                rtp_header->header.headerLength,
                rtp_header->header.paddingLength);
    log_verbose("tag", "Recevied payload data addr: %p size: %d\n", data, size);
    uint32_t writed = fwrite(data + rtp_header->header.headerLength,
                             1, size - rtp_header->header.headerLength, _fp);
    fflush(_fp);
    CHECK_EQ(writed, size - rtp_header->header.headerLength);
    log_verbose("R-RTP", "Recieve RTP stream size: %d\n", writed);
    return 0;
  }
  std::list<uint16_t> _sequence_numbers;
  FILE *_fp;
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
  explicit MockTransport(uint32_t rtx_ssrc, char *ipsrc, char *ipdst)
      : _count(0),
        _packet_loss(0),
        _consecutive_drop_start(0),
        _consecutive_drop_end(0),
        _rtx_ssrc(rtx_ssrc),
        _count_rtx_ssrc(0),
        _module(NULL) {
    strcpy(_ipsrc, ipsrc);
    strcpy(_ipdst, ipdst);
    logi("IP src: %s IP dst: %s\n", _ipsrc, ipdst);
  }

  virtual bool SendRtp(const uint8_t* packet,
                       size_t length,
                       const PacketOptions& options) {
    logv("SendRtp idx: %08d addr: %p size: %d\n", _count, packet, length);
    _count++;
    socket_addr sock_dst_addr;
    sock_dst_addr._sockaddr_in.sin_family = AF_INET;
    sock_dst_addr._sockaddr_in.sin_port = htons(25050);
    sock_dst_addr._sockaddr_in.sin_addr = inet_addr(_ipdst);

    int32_t ret = _rtp_sock->SendTo((const int8_t*)packet, length, sock_dst_addr);
    logv("SendRTP data size: %d\n", ret);

    return true;
  }
  virtual bool SendRtcp(const uint8_t* packet, size_t length) {
    logw("SendRtp idx: %08d addr: %p size: %d\n", _count, packet, length);
    _count++;
    socket_addr sock_dst_addr;
    sock_dst_addr._sockaddr_in.sin_family = AF_INET;
    sock_dst_addr._sockaddr_in.sin_port = htons(25051);
    sock_dst_addr._sockaddr_in.sin_addr = inet_addr(_ipdst);

    int32_t ret = _rtcp_sock->SendTo((const int8_t*)packet, length, sock_dst_addr);
    logw("SendRTCP data size: %d\n", ret);
    return true;
  }
  void SetSendModule(RtpRtcp * module) {
    _module = module;

    uint8_t threads = 1;
    _sock_mgr = SocketManager::Create(0, threads);
    _rtp_sock = Socket::CreateSocket(0, _sock_mgr,
                                      this,
                                      incomingSocketCallback);

    _rtcp_sock = Socket::CreateSocket(0, _sock_mgr,
                                      this,
                                      incomingRTCPSocketCallback);
    socket_addr sock_addr;
    sock_addr._sockaddr_in.sin_family = AF_INET;
    sock_addr._sockaddr_in.sin_port = htons(15050+1);
    sock_addr._sockaddr_in.sin_addr = inet_addr(_ipsrc);

    CHECK_EQ(true, _rtcp_sock->Bind(sock_addr));
    CHECK_EQ(true, _rtcp_sock->StartReceiving());
  }

  static void incomingSocketCallback(CallbackObj obj, const int8_t* buf,
                                      int32_t len, const socket_addr* from) {
  }
  static void incomingRTCPSocketCallback(CallbackObj obj, const int8_t* buf,
                                      int32_t len, const socket_addr* from) {
    MockTransport *pThis = static_cast<MockTransport*>(obj);
    const uint8_t *rtcp_packet = (const uint8_t*)buf;
    int32_t packet_length = len;

    int32_t ret = pThis->_module->IncomingRtcpPacket(rtcp_packet, packet_length);
    if (ret) {
      loge("incomingRTCPSocketCallback error\n");
    }
    logw("incoming rtcp buf[%p] size[%d] from[%p]\n", buf, len, from);
  }

private:
  int32_t _count;
  int32_t _packet_loss;
  int32_t _consecutive_drop_start;
  int32_t _consecutive_drop_end;
  uint32_t _rtx_ssrc;
  int32_t _count_rtx_ssrc;

  RtpRtcp *_module;
  //udp transport implement
  SocketManager *_sock_mgr;
  Socket *_rtp_sock;
  Socket *_rtcp_sock;
  char _ipsrc[32];
  char _ipdst[32];
};

class RtcpPacketTypeCounterObserverImpl : public RtcpPacketTypeCounterObserver {
 public:
  RtcpPacketTypeCounterObserverImpl() : ssrc_(0) {}
  virtual ~RtcpPacketTypeCounterObserverImpl() {}
  void RtcpPacketTypesCounterUpdated(
      uint32_t ssrc,
      const RtcpPacketTypeCounter& packet_counter) override {
    ssrc_ = ssrc;
    counter_ = packet_counter;
  }
  uint32_t ssrc_;
  RtcpPacketTypeCounter counter_;
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
  RtpRtcp *rtp_rtcp;
  uint8_t *csd;
  uint32_t csd_size;
}OMXContext;

OMX_ERRORTYPE sEventHandler(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_IN OMX_PTR pAppData,
  OMX_IN OMX_EVENTTYPE eEvent,
  OMX_IN OMX_U32 nData1,
  OMX_IN OMX_U32 nData2,
  OMX_IN OMX_PTR pEventData) {
    //logv("EventHandler com: %p appdata: %p\n", hComponent, pAppData);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE sEmptyBufferDone(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_IN OMX_PTR pAppData,
  OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) {
    //logv("EmptyBufferDone com: %p appdata: %p\n", hComponent, pAppData);
    OMXContext *omx_ctx = (OMXContext *)pAppData;
    int32_t status = cirq_enqueue(omx_ctx->ebd, pBuffer);
    CHECK_EQ(0, status);

    return OMX_ErrorNone;
}
OMX_ERRORTYPE sFillBufferDone(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_IN OMX_PTR pAppData,
  OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) {
    //logv("FillBufferDone com: %p appdata: %p\n", hComponent, pAppData);
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
    pBuffer->nTimeStamp = omx_ctx->ts * 1000;

    uint32_t readed = fread(pBuffer->pBuffer, 1, pBuffer->nFilledLen, omx_ctx->fp);
    if (readed < pBuffer->nFilledLen) {
      rewind(omx_ctx->fp);
      readed = fread(pBuffer->pBuffer, 1, pBuffer->nFilledLen, omx_ctx->fp);
      CHECK_EQ(readed, pBuffer->nFilledLen);
    }
    oRet = OMX_EmptyThisBuffer(hComponent, pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
    //logi("EmptyThisBuffer: %p\n", pBuffer);
    omx_ctx->ts += 33;
  }

  pdata = NULL;
  cirq_dequeue(omx_ctx->fbd, &pdata);
  pBuffer = (OMX_BUFFERHEADERTYPE*)pdata;
  if (pBuffer) {
    if (pBuffer->nFilledLen > 0) {

      RTPFragmentationHeader fragment;
      if (pBuffer->nFlags == OMX_BUFFERFLAG_CODECCONFIG) {
        memcpy(omx_ctx->csd, pBuffer->pBuffer+ pBuffer->nOffset, pBuffer->nFilledLen);
        omx_ctx->csd_size = pBuffer->nFilledLen;
      } else {
        if (pBuffer->nFlags == OMX_BUFFERFLAG_SYNCFRAME) {
          if (omx_ctx->csd_size <= pBuffer->nOffset) {
            logi("Add csd header for SYNCFrame size:%d\n", omx_ctx->csd_size);
            pBuffer->nOffset -=  omx_ctx->csd_size;
            pBuffer->nFilledLen += omx_ctx->csd_size;
            memcpy(pBuffer->pBuffer + pBuffer->nOffset, omx_ctx->csd, omx_ctx->csd_size);
          }
        }

        fragment.VerifyAndAllocateFragmentationHeader(1);
        fragment.fragmentationOffset[0] = 4;
        fragment.fragmentationLength[0] = pBuffer->nFilledLen - 4;
        fragment.fragmentationTimeDiff[0] = 0;
        fragment.fragmentationPlType[0] = 0;

        logv("pBuffer: %p nFilledLen: %u nSize: %u nOffset: %u nFlags: %d\n",
            pBuffer, pBuffer->nFilledLen, pBuffer->nAllocLen, pBuffer->nOffset,
            pBuffer->nFlags);
        logv("SendOutgoingData: %p size: %lu ts: %llu\n",
              pBuffer->pBuffer, pBuffer->nFilledLen, pBuffer->nTimeStamp);
        int32_t ret = omx_ctx->rtp_rtcp->SendOutgoingData(
                       pBuffer->nFlags == OMX_BUFFERFLAG_SYNCFRAME ?
                          kVideoFrameKey: kVideoFrameDelta,
                       kPayloadType, (pBuffer->nTimeStamp / 1000)*90,
                       pBuffer->nTimeStamp / 1000, pBuffer->pBuffer + pBuffer->nOffset,
                       pBuffer->nFilledLen, &fragment, NULL);
        CHECK_EQ(0, ret);
        #if 1
        //Save raw encoded stream
        uint32_t writed = fwrite(pBuffer->pBuffer + pBuffer->nOffset, 1,
                            pBuffer->nFilledLen, omx_ctx->fp_encoded);
        CHECK_EQ(writed, pBuffer->nFilledLen);
        writed = fprintf(omx_ctx->fp_len, "%d\n", (int32_t)pBuffer->nFilledLen);
        CHECK_GT(writed, (uint32_t)0);
        fflush(omx_ctx->fp_encoded);
        fflush(omx_ctx->fp_len);
        #endif
      }
    }

    pBuffer->nFlags = 0;
    pBuffer->nFilledLen = 0;
    pBuffer->nOffset = 24;
    pBuffer->nTimeStamp = 0;
    oRet = OMX_FillThisBuffer(hComponent, pBuffer);

    CHECK_EQ(oRet, OMX_ErrorNone);
  }
  os_msleep(33);
  return true;
}

void DumpPortDefine(OMX_PARAM_PORTDEFINITIONTYPE *def) {
    logv("-----------------Port Define---------------\n");
    logv("Index: %d\n", def->nPortIndex);
    logv("Dir: %s\n", def->eDir == OMX_DirInput ? "DirInput" : "DirOuput");
    logv("BufMinCount: %u\n", def->nBufferCountMin);
    logv("BufActCount: %u\n", def->nBufferCountActual);
    logv("Domain: %s\n", def->eDomain == OMX_PortDomainVideo ? "Video" : "Audio");
    if (def->eDomain == OMX_PortDomainVideo) {
      logv("Width: %u\n", def->format.video.nFrameWidth);
      logv("Height: %u\n", def->format.video.nFrameHeight);
      logv("Stride: %u\n", def->format.video.nStride);
      logv("SliceH: %u\n", def->format.video.nSliceHeight);
      logv("Bitrate: %u\n", def->format.video.nBitrate);
      logv("Framerate: %d\n", def->format.video.xFramerate >> 16);
      logv("BufAlign: %u\n", def->nBufferAlignment);
      logv("CompressFmt: %x\n", def->format.video.eCompressionFormat);
      logv("ColorFmt: %x\n", def->format.video.eColorFormat);
    }
}

int32_t main(int argc, char *argv[]) {
  log_setlevel(eLogWarn);
  if (argc < 3) {
    logv("Usage: ./media_video_utest 192.168.1.0 192.168.1.100 \n");
    return 0;
  }
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
  omx_ctx->csd = (uint8_t*)malloc(64);
  omx_ctx->csd_size = 0;

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
  omx_ctx->fp_encoded = fopen("rtp_snd.h264", "w+");
  omx_ctx->fp_len = fopen("rtp_snd.len", "w+");

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
  def.format.video.xFramerate = 15 << 16;
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
  def.format.video.nBitrate = 500;
  def.format.video.xFramerate = 0;
  def.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  def.format.video.eColorFormat = OMX_COLOR_FormatUnused;
  DumpPortDefine(&def);
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);

  OMX_VIDEO_PARAM_AVCTYPE avc_type;
  avc_type.nPortIndex = 1;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamVideoAvc, &avc_type);
  CHECK_EQ(oRet, OMX_ErrorNone);
  avc_type.nPFrames = 150;
  avc_type.nBFrames = 0;
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamVideoAvc, &avc_type);
  CHECK_EQ(oRet, OMX_ErrorNone);

  OMX_VIDEO_CONFIG_AVCINTRAPERIOD avc_period;
  avc_period.nPortIndex = 1;
  avc_period.nIDRPeriod = 150;
  oRet = OMX_SetConfig(pHandle, OMX_IndexConfigVideoAVCIntraPeriod, &avc_period);
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

  RtpRtcp::Configuration configuration;
  configuration.audio = false;
  configuration.receiver_only = false;
  configuration.clock = Clock::GetRealTimeClock();
  configuration.receive_statistics = ReceiveStatistics::Create(configuration.clock);
  configuration.outgoing_transport = new MockTransport(1243, argv[1], argv[2]);
  configuration.intra_frame_callback = NULL;
  configuration.bandwidth_callback = NULL;
  configuration.transport_feedback_callback = NULL;
  configuration.rtt_stats = NULL;
  configuration.rtcp_packet_type_counter_observer = NULL;
  configuration.remote_bitrate_estimator = NULL;
  configuration.paced_sender = NULL;
  configuration.transport_sequence_number_allocator = NULL;
  configuration.send_bitrate_observer = NULL;
  configuration.send_frame_count_observer = NULL;
  configuration.send_side_delay_observer = NULL;
  configuration.event_log = NULL;
  configuration.send_packet_observer = NULL;
  configuration.retransmission_rate_limiter = NULL;

  RtpRtcp* rtpRtcp = RtpRtcp::CreateRtpRtcp(configuration);
  omx_ctx->rtp_rtcp = rtpRtcp;

  ((MockTransport*)configuration.outgoing_transport)->SetSendModule(rtpRtcp);
  rtpRtcp->RegisterVideoSendPayload(kPayloadType, "H264");
  rtpRtcp->SetStartTimestamp(123654);
  rtpRtcp->SetSequenceNumber(1234);
  rtpRtcp->SetSSRC(kTestSsrc);
  rtpRtcp->SetSendingStatus(true);
  rtpRtcp->SetSendingMediaStatus(true);

  uint32_t tid = 0;
  omx_ctx->thread->start(tid);

  getchar();

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

  free(omx_ctx->csd);
  free(omx_ctx);
  return 0;
}