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
#include <protocol_receive_statistics.h>
#include <protocol_rtp_receiver.h>
#include <protocol_rtp_header_parser.h>
#include <protocol_rtp_payload_registry.h>

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
    _last_seq_num = 0;
    _fp = fopen("rtp_recv.h264", "w+");
  }

  int32_t OnReceivedPayloadData(
      const uint8_t* data,
      size_t size,
      const protocol::WebRtcRTPHeader* rtp_header) override {
#if 0
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
#endif
    if (_last_seq_num != 0) {
      CHECK(rtp_header->header.sequenceNumber == _last_seq_num + 1);
    }
    _last_seq_num = rtp_header->header.sequenceNumber;
    log_verbose("tag", "Recevied payload data addr: %p size: %d\n", data, size);
    uint8_t start_code[4] = {0x00, 0x00, 0x00, 0x01};
    uint32_t writed = fwrite(start_code, 1, 4, _fp);
    writed = fwrite(data + rtp_header->header.headerLength,
                             1, size - rtp_header->header.headerLength, _fp);
    fflush(_fp);
    CHECK_EQ(writed, size - rtp_header->header.headerLength);
    log_verbose("R-RTP", "Recieve RTP stream size: %d\n", writed);
    return 0;
  }
  uint16_t _last_seq_num;
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
        _count_rtx_ssrc(0)
        {
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
  void SetSendModule(
                    RTPPayloadRegistry* rtp_payload_registry,
                    RtpReceiver* receiver) {
    _rtp_payload_registry = rtp_payload_registry;
    _rtp_receiver = receiver;

    uint8_t threads = 1;
    _sock_mgr = SocketManager::Create(0, threads);

    _rtp_sock = Socket::CreateSocket(0, _sock_mgr,
                                      this,
                                      incomingSocketCallback);
    socket_addr sock_addr;
    sock_addr._sockaddr_in.sin_family = AF_INET;
    sock_addr._sockaddr_in.sin_port = htons(25050);
    sock_addr._sockaddr_in.sin_addr = inet_addr(_ipsrc);

    CHECK_EQ(true, _rtp_sock->Bind(sock_addr));
    CHECK_EQ(true, _rtp_sock->StartReceiving());

    _rtcp_sock = Socket::CreateSocket(0, _sock_mgr,
                                      this,
                                      incomingRTCPSocketCallback);

    sock_addr._sockaddr_in.sin_family = AF_INET;
    sock_addr._sockaddr_in.sin_port = htons(25050+1);
    sock_addr._sockaddr_in.sin_addr = inet_addr(_ipsrc);

    CHECK_EQ(true, _rtcp_sock->Bind(sock_addr));
    CHECK_EQ(true, _rtcp_sock->StartReceiving());
  }

  static void incomingSocketCallback(CallbackObj obj, const int8_t* buf,
                                      int32_t len, const socket_addr* from) {
    logv("incomingRTPSocketCallback buf: %p size: %d\n",
        buf, len);
    MockTransport *pThis = static_cast<MockTransport*>(obj);
    const uint8_t *ptr = (const uint8_t*)buf;
    int32_t packet_length = len;
    RTPHeader header;
    std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
    if (!parser->Parse(ptr, len, &header)) {
      loge("RtpHeaderParser error!\n");
      return;
    }
    PayloadUnion payload_specific;
    if (!pThis->_rtp_payload_registry->GetPayloadSpecifics(header.payloadType,
                                                    &payload_specific)) {
      loge("GetPayloadSpecifics error!\n");
      return;
    }

    if (!pThis->_rtp_receiver->IncomingRtpPacket(header, ptr,
                                          packet_length,
                                          payload_specific, true)) {
      logv("incomingRTPSocketCallback error\n");
    }
  }
  static void incomingRTCPSocketCallback(CallbackObj obj, const int8_t* buf,
                                      int32_t len, const socket_addr* from) {
    logw("incoming rtcp buf[%p] size[%d] from[%p]\n", buf, len, from);
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
  //RTCPReceiver* _rtcp_receiver;
  //RTCPSender* _rtcp_sender;

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

int32_t main(int argc, char *argv[]) {
  log_setlevel(eLogVerbose);
  if (argc < 3) {
    logv("Usage: ./media_vidrecv_utest 192.168.1.0 192.168.1.100 \n");
    return 0;
  }

  Clock* clock = Clock::GetRealTimeClock();
  scoped_ptr<RTPPayloadRegistry> rtp_payload_registry(new RTPPayloadRegistry(
                                RTPPayloadStrategy::CreateStrategy(false)));
  scoped_ptr<VerifyingRtxReceiver> receiver(new VerifyingRtxReceiver());
  scoped_ptr<TestRtpFeedback> rtp_feedback(new TestRtpFeedback());
  scoped_ptr<RtpReceiver> rtp_receiver(RtpReceiver::CreateVideoReceiver(
    clock, receiver.get(), rtp_feedback.get(), rtp_payload_registry.get()));
  bool created_new_payload = false;
  int32_t ret = rtp_payload_registry->RegisterReceivePayload(
                      "H264",
                      kPayloadType,
                      90000,
                      1,
                      0, &created_new_payload);
  CHECK_EQ(0, ret);

  scoped_ptr<MockTransport> transport(new MockTransport(1243, argv[1], argv[2]));
  transport->SetSendModule(rtp_payload_registry.get(),
                           rtp_receiver.get());

  getchar();

  return 0;
}
