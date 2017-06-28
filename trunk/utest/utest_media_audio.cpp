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
const int kPayloadType = 97;
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
    _fp = fopen("recv_rtp.aac", "w+");
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
        _rtp_payload_registry(NULL),
        _rtp_receiver(NULL),
        _rtp_sender(NULL) {
    strcpy(_ipsrc, ipsrc);
    strcpy(_ipdst, ipdst);
    loge("IP src: %s IP dst: %s\n", _ipsrc, ipdst);
  }

  virtual bool SendRtp(const uint8_t* packet,
                       size_t length,
                       const PacketOptions& options) {
    logv("SendRtp idx: %08d addr: %p size: %d\n", _count, packet, length);
    _count++;
    socket_addr sock_dst_addr;
    sock_dst_addr._sockaddr_in.sin_family = AF_INET;
    sock_dst_addr._sockaddr_in.sin_port = htons(25060);
    sock_dst_addr._sockaddr_in.sin_addr = inet_addr(_ipdst);

    //DumpRTPHeader((uint8_t*)packet);
    logi("%02x %02x %02x\n", *(packet + 12 + 0), *(packet + 12 + 1), *(packet + 12 + 2));

    int32_t ret = _rtp_sock->SendTo((const int8_t*)packet, length, sock_dst_addr);
    logv("SendRTP data size: %d\n", ret);

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

    uint8_t threads = 1;
    _sock_mgr = SocketManager::Create(0, threads);
    _rtp_sock = Socket::CreateSocket(0, _sock_mgr,
                                      this,
                                      incomingSocketCallback);
    socket_addr sock_addr;
    sock_addr._sockaddr_in.sin_family = AF_INET;
    sock_addr._sockaddr_in.sin_port = htons(15060);
    sock_addr._sockaddr_in.sin_addr = inet_addr(_ipsrc);

    CHECK_EQ(true, _rtp_sock->Bind(sock_addr));
    CHECK_EQ(true, _rtp_sock->StartReceiving());
  }
  static void incomingSocketCallback(CallbackObj obj, const int8_t* buf,
                                      int32_t len, const socket_addr* from) {
    MockTransport *pThis = static_cast<MockTransport*>(obj);
    const uint8_t *ptr = (const uint8_t*)buf;
    int32_t packet_length = len;
    RTPHeader header;
    std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
    if (!parser->Parse(ptr, len, &header)) {
      return;
    }
    PayloadUnion payload_specific;
    if (!pThis->_rtp_payload_registry->GetPayloadSpecifics(header.payloadType,
                                                    &payload_specific)) {
      return;
    }

    if (!pThis->_rtp_receiver->IncomingRtpPacket(header, ptr,
                                          packet_length,
                                          payload_specific, true)) {
      logv("incomingRTPSocketCallback error\n");
    }

    //logv("incoming buf[%p] size[%d] from[%p]\n", buf, len, from);
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

  //udp transport implement
  SocketManager *_sock_mgr;
  Socket *_rtp_sock;

  char _ipsrc[32];
  char _ipdst[32];
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

/*
 * ADTS (Audio data transport stream) header structure.
 * It consists of 7 or 9 bytes (with or without CRC):
 * 12 bits of syncword 0xFFF, all bits must be 1
 * 1 bit of field ID. 0 for MPEG-4, and 1 for MPEG-2
 * 2 bits of MPEG layer. If in MPEG-TS, set to 0
 * 1 bit of protection absense. Set to 1 if no CRC.
 * 2 bits of profile code. Set to 1 (The MPEG-4 Audio
 *   object type minus 1. We are using AAC-LC = 2)
 * 4 bits of sampling frequency index code (15 is not allowed)
 * 1 bit of private stream. Set to 0.
 * 3 bits of channel configuration code. 0 resevered for inband PCM
 * 1 bit of originality. Set to 0.
 * 1 bit of home. Set to 0.
 * 1 bit of copyrighted steam. Set to 0.
 * 1 bit of copyright start. Set to 0.
 * 13 bits of frame length. It included 7 ot 9 bytes header length.
 *   it is set to (protection absense? 7: 9) + size(AAC frame)
 * 11 bits of buffer fullness. 0x7FF for VBR.
 * 2 bits of frames count in one packet. Set to 0.
 */
int32_t aac_add_adts_header(uint32_t frame_len, uint8_t *dst) {
    uint8_t *dst_cur = dst;
    uint8_t data = 0xFF;
    memcpy(dst_cur, &data, 1);
    dst_cur++;

    const uint8_t kFieldId = 0;
    const uint8_t kMpegLayer = 0;
    const uint8_t kProtectionAbsense = 1;  // 1: kAdtsHeaderLength = 7
    data = 0xF0;
    data |= (kFieldId << 3);
    data |= (kMpegLayer << 1);
    data |= kProtectionAbsense;
    memcpy(dst_cur, &data, 1);
    dst_cur++;

    const uint8_t kProfileCode = OMX_AUDIO_AACObjectLC - 1;
    uint8_t kSampleFreqIndex = 5;/*32kHZ*/
    const uint8_t kPrivateStream = 0;
    const uint8_t kChannelConfigCode = 1/*1: mono 2: stero*/;
    data = (kProfileCode << 6);
    data |= (kSampleFreqIndex << 2);
    data |= (kPrivateStream << 1);
    data |= (kChannelConfigCode >> 2);
    memcpy(dst_cur, &data, 1);
    dst_cur++;

    // 4 bits from originality to copyright start
    const uint8_t kCopyright = 0;
    const uint32_t kFrameLength = frame_len;
    data = ((kChannelConfigCode & 3) << 6);
    data |= (kCopyright << 2);
    data |= ((kFrameLength & 0x1800) >> 11);
    memcpy(dst_cur, &data, 1);
    dst_cur++;

    data = ((kFrameLength & 0x07F8) >> 3);
    memcpy(dst_cur, &data, 1);
    dst_cur++;

    const uint32_t kBufferFullness = 0x7FF;  // VBR
    data = ((kFrameLength & 0x07) << 5);
    data |= ((kBufferFullness & 0x07C0) >> 6);
    memcpy(dst_cur, &data, 1);
    dst_cur++;

    const uint8_t kFrameCount = 0;
    data = ((kBufferFullness & 0x03F) << 2);
    data |= kFrameCount;
    memcpy(dst_cur, &data, 1);
    dst_cur++;

    return 0;
}


bool audio_encoder_loop(void *ctx) {
  OMX_ERRORTYPE oRet = OMX_ErrorNone;
  OMXContext *omx_ctx = (OMXContext *)ctx;

  OMX_HANDLETYPE hComponent = omx_ctx->hComponent;
  void *pdata = NULL;
  cirq_dequeue(omx_ctx->ebd, &pdata);
  int32_t audio_frame_time = 10;//ms
  OMX_BUFFERHEADERTYPE* pBuffer = (OMX_BUFFERHEADERTYPE*)pdata;
  if (pBuffer) {
    pBuffer->nFlags = 0;
    pBuffer->nFilledLen = 32 * audio_frame_time * sizeof(int16_t) * 1;
    pBuffer->nOffset = 0;
    pBuffer->nTimeStamp = (omx_ctx->ts * 1000);

    uint32_t readed = fread(pBuffer->pBuffer, 1, pBuffer->nFilledLen, omx_ctx->fp);
    if (readed < pBuffer->nFilledLen) {
      rewind(omx_ctx->fp);
      readed = fread(pBuffer->pBuffer, 1, pBuffer->nFilledLen, omx_ctx->fp);
      CHECK_EQ(readed, pBuffer->nFilledLen);
    }

    oRet = OMX_EmptyThisBuffer(hComponent, pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
    logv("EmptyThisBuffer: %p\n", pBuffer);
    omx_ctx->ts += audio_frame_time;
  }

  pdata = NULL;
  cirq_dequeue(omx_ctx->fbd, &pdata);
  pBuffer = (OMX_BUFFERHEADERTYPE*)pdata;
  if (pBuffer ) {
    if (pBuffer->nFilledLen > 0 && pBuffer->nFlags == OMX_BUFFERFLAG_CODECCONFIG) {
      loge("CSD: %02x %02x\n", *(pBuffer->pBuffer + pBuffer->nOffset),
            *(pBuffer->pBuffer + pBuffer->nOffset + 1));
    }
    bool isCsd = pBuffer->nFlags == OMX_BUFFERFLAG_CODECCONFIG;
    if (pBuffer->nFilledLen > 0 && !isCsd) {
      #if 0
      uint32_t frame_len = 7 + pBuffer->nFilledLen;
      if (pBuffer->nOffset >= 7) {
        pBuffer->nOffset -= 7;
        pBuffer->nFilledLen += 7;
        aac_add_adts_header(frame_len, pBuffer->pBuffer + pBuffer->nOffset);
      }

      logw("Buffer flag: %08x\n", pBuffer->nFlags);
      uint32_t writed = fwrite(pBuffer->pBuffer, 1, pBuffer->nFilledLen, omx_ctx->fp_encoded);
      CHECK_EQ(writed, pBuffer->nFilledLen);
      writed = fprintf(omx_ctx->fp_len, "%d\n", (int32_t)pBuffer->nFilledLen);
      CHECK_GT(writed, (uint32_t)0);
      fflush(omx_ctx->fp_encoded);
      fflush(omx_ctx->fp_len);
      #endif

      pBuffer->nOffset -= 4;
      pBuffer->nFilledLen += 4;
      int16_t adts_size = pBuffer->nFilledLen - 4;
      loge("adts_size: %d\n", adts_size);
      *(pBuffer->pBuffer + pBuffer->nOffset + 0) = 0x00;
      *(pBuffer->pBuffer + pBuffer->nOffset + 1) = 0x10;
      *(pBuffer->pBuffer + pBuffer->nOffset + 2) = ((uint8_t)(adts_size & 0x1fe0)) >> 5;
      *(pBuffer->pBuffer + pBuffer->nOffset + 3) = ((uint8_t)(adts_size & 0x1f)) << 3;
      loge("%02x %02x %02x %02x %02x %02x\n",
          *(pBuffer->pBuffer + pBuffer->nOffset + 0),
          *(pBuffer->pBuffer + pBuffer->nOffset + 1),
          *(pBuffer->pBuffer + pBuffer->nOffset + 2),
          *(pBuffer->pBuffer + pBuffer->nOffset + 3),
          *(pBuffer->pBuffer + pBuffer->nOffset + 4),
          *(pBuffer->pBuffer + pBuffer->nOffset + 5));
      //Send to network
      //RTPFragmentationHeader fragment;
      //fragment.VerifyAndAllocateFragmentationHeader(1);
      //fragment.fragmentationOffset[0] = 0;
      //fragment.fragmentationLength[0] = pBuffer->nFilledLen;
      //fragment.fragmentationTimeDiff[0] = 0;
      //fragment.fragmentationPlType[0] = kPayloadType;

      logw("SendOutgoingData: %p size: %lu ts: %llu(ms)\n",
            pBuffer->pBuffer, pBuffer->nFilledLen, pBuffer->nTimeStamp/1000);
      //logi("Buffer flags: %08x\n", pBuffer->nFlags);
      int32_t ret = omx_ctx->rtp_sender->SendOutgoingData(
                     kAudioFrameSpeech,
                     kPayloadType, (pBuffer->nTimeStamp*32)/1000, -1/*capture_time_ms*/,
                     pBuffer->pBuffer + pBuffer->nOffset,
                     pBuffer->nFilledLen, NULL/*&fragment*/, NULL);
      CHECK_EQ(0, ret);
    }
    pBuffer->nFlags = 0;
    pBuffer->nFilledLen = 0;
    pBuffer->nOffset = 11;
    pBuffer->nTimeStamp = 0;
    oRet = OMX_FillThisBuffer(hComponent, pBuffer);
    logv("FillThisBuffer: %p\n", pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }
  os_msleep(audio_frame_time);
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
  log_setlevel(eLogInfo);
  if (argc < 3) {
    logv("Usage: ./media_audio_utest 192.168.1.0 192.168.1.100 \n");
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
  int32_t status = -1;
  status = cirq_create(&omx_ctx->fbd, 4);
  CHECK_EQ(0, status);
  status = cirq_create(&omx_ctx->ebd, 4);
  CHECK_EQ(0, status);
  omx_ctx->thread = os::Thread::Create(audio_encoder_loop, omx_ctx);
  omx_ctx->ts = 3600;
  omx_ctx->encode = true;
  omx_ctx->fp = fopen("32k-mono-16bw-all-your-life.pcm", "r");
  omx_ctx->fp_encoded = fopen("32k-mono-16width.aac", "w+");
  omx_ctx->fp_len = fopen("32k-mono-16width-aac.len", "w+");

  OMX_U32 numChannels = 1;
  OMX_U32 sampleRate = 32000;
  OMX_U32 bitRate = 32000;

  OMX_CALLBACKTYPE omx_cb;
  omx_cb.EventHandler = sEventHandler;
  omx_cb.FillBufferDone = sFillBufferDone;
  omx_cb.EmptyBufferDone = sEmptyBufferDone;
  OMX_HANDLETYPE pHandle;
  oRet = OMX_GetHandle(&pHandle,
    (OMX_STRING)"OMX.omxil.aac.encoder",
    (OMX_PTR)omx_ctx,
    &omx_cb);
  CHECK_EQ(oRet, OMX_ErrorNone);
  omx_ctx->hComponent = pHandle;

  OMX_PARAM_PORTDEFINITIONTYPE def;
  InitOMXParams(&def);
  def.nPortIndex = 0;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);
  def.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  DumpPortDefine(&def);
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);

  def.nPortIndex = 1;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);
  def.format.audio.bFlagErrorConcealment = OMX_TRUE;
  def.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
  DumpPortDefine(&def);
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);

  OMX_AUDIO_PARAM_PCMMODETYPE pcmParams;
  InitOMXParams(&pcmParams);
  pcmParams.nPortIndex = 0;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamAudioPcm, &pcmParams);
  CHECK_EQ(oRet, OMX_ErrorNone);
  pcmParams.nChannels = numChannels;
  pcmParams.eNumData = OMX_NumericalDataSigned;
  pcmParams.bInterleaved = OMX_TRUE;
  pcmParams.nBitPerSample = 16;
  pcmParams.nSamplingRate = sampleRate;
  pcmParams.ePCMMode = OMX_AUDIO_PCMModeLinear;
  pcmParams.eChannelMapping[0]=OMX_AUDIO_ChannelCF;
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamAudioPcm, &pcmParams);
  CHECK_EQ(oRet, OMX_ErrorNone);

  OMX_AUDIO_PARAM_AACPROFILETYPE profile;
  //InitOMXParams(&profile);
  //profile.nPortIndex = 0;
  //oRet = OMX_GetParameter(pHandle, OMX_IndexParamAudioAac, &profile);
  //profile.nChannels = numChannels;
  //profile.nSampleRate = sampleRate;
  //profile.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4FF;
  //oRet = OMX_SetParameter(pHandle, OMX_IndexParamAudioAac, &profile);
  //CHECK_EQ(oRet, OMX_ErrorNone);

  //OMX_AUDIO_PARAM_ANDROID_AACPRESENTATIONTYPE presentation;
  //presentation.nMaxOutputChannels = 6;
  //presentation.nDrcCut = -1;
  //presentation.nDrcBoost = -1;
  //presentation.nHeavyCompression = -1;
  //presentation.nTargetReferenceLevel = -1;
  //presentation.nEncodedTargetLevel = -1;
  //presentation.nPCMLimiterEnable = -1;
  //oRet = OMX_SetParameter(pHandle, (OMX_INDEXTYPE)OMX_IndexParamAudioAndroidAacPresentation, &presentation);
  //CHECK_EQ(oRet, OMX_ErrorNone);

  //OMX_AUDIO_PARAM_AACPROFILETYPE profile;

  InitOMXParams(&profile);
  profile.nPortIndex = 1;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamAudioAac, &profile);
  CHECK_EQ(oRet, OMX_ErrorNone);
  profile.nChannels = numChannels;
  profile.eChannelMode = OMX_AUDIO_ChannelModeMono;//OMX_AUDIO_ChannelModeStereo
  profile.nSampleRate = sampleRate;
  profile.nBitRate = bitRate;
  profile.nAudioBandWidth = 0;
  profile.nFrameLength = 0;
  profile.nAACtools = OMX_AUDIO_AACToolAll;
  profile.nAACERtools = OMX_AUDIO_AACERNone;
  profile.eAACProfile = (OMX_AUDIO_AACPROFILETYPE)OMX_AUDIO_AACObjectLC;
  profile.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4FF;
  profile.nAACtools |= OMX_AUDIO_AACToolAndroidSSBR | OMX_AUDIO_AACToolAndroidDSBR;
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamAudioAac, &profile);
  CHECK_EQ(oRet, OMX_ErrorNone);

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < 4; ++i) {
    oRet = OMX_AllocateBuffer(pHandle, &omx_ctx->inBuffer[i], 0, NULL, 32 * 110 * sizeof(int16_t) * 2);
    CHECK_EQ(oRet, OMX_ErrorNone);
    CHECK(omx_ctx->inBuffer[i]);
    logv("Port: %d index: %d Allocate Buffer: %p len: %d\n",
        0, i, omx_ctx->inBuffer[i]->pBuffer, omx_ctx->inBuffer[i]->nFilledLen);
  }

  for (int32_t i = 0; i < 4; ++i) {
    oRet = OMX_AllocateBuffer(pHandle, &omx_ctx->outBuffer[i], 1, NULL, 8192);
    CHECK_EQ(oRet, OMX_ErrorNone);
    CHECK(omx_ctx->outBuffer[i]);
    logv("Port: %d index: %d Allocate Buffer: %p len: %d\n",
        1, i, omx_ctx->outBuffer[i]->pBuffer, omx_ctx->outBuffer[i]->nFilledLen);
  }
  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < 4; ++i) {
    int32_t status = cirq_enqueue(omx_ctx->ebd, omx_ctx->inBuffer[i]);
    CHECK_EQ(0, status);
    status = cirq_enqueue(omx_ctx->fbd, omx_ctx->outBuffer[i]);
    CHECK_EQ(0, status);
  }

  int32_t ret = -1;
  bool audio __attribute((__unused__)) = true;
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

  scoped_ptr<MockTransport> transport(new MockTransport(1243, argv[1], argv[2]));
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

  scoped_ptr<RTPPayloadRegistry> rtp_payload_registry(new RTPPayloadRegistry(
                                RTPPayloadStrategy::CreateStrategy(audio)));
  scoped_ptr<VerifyingRtxReceiver> receiver(new VerifyingRtxReceiver());
  scoped_ptr<TestRtpFeedback> rtp_feedback(new TestRtpFeedback());
  scoped_ptr<RtpReceiver> rtp_receiver(RtpReceiver::CreateAudioReceiver(
    clock, receiver.get(), rtp_feedback.get(), rtp_payload_registry.get()));

  transport->SetSendModule(NULL,
                    rtp_payload_registry.get(),
                    rtp_receiver.get());

  rtp_sender->SetSSRC(kTestSsrc);
  ret = rtp_sender->RegisterPayload("AAC", kPayloadType, sampleRate, numChannels, bitRate);
  CHECK_EQ(0, ret);
  rtp_payload_registry->SetRtxSsrc(kTestSsrc + 1);
  bool created_new_payload = false;
  ret = rtp_payload_registry->RegisterReceivePayload(
                      "AAC",
                      kPayloadType,
                      sampleRate,
                      numChannels,
                      bitRate, &created_new_payload);
  CHECK_EQ(0, ret);
  omx_ctx->rtp_sender = rtp_sender.get();

  uint32_t tid = 0;
  omx_ctx->thread->start(tid);
  os_sleep(1000);

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < 4; ++i) {
    oRet = OMX_FreeBuffer(pHandle, 0, omx_ctx->inBuffer[i]);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }
  for (int32_t i = 0; i < 4; ++i) {
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
  fclose(omx_ctx->fp_len);

  free(omx_ctx);
  return 0;
}
