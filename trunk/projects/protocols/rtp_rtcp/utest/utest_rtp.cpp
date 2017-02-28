#include <protocol_rtp_sender.h>
#include <protocol_rtp_receiver.h>
#include <protocol_rtp_payload_registry.h>
#include <protocol_rtp_header_parser.h>

#include <os_typedefs.h>
#include <os_mutex.h>
#include <os_assert.h>
#include <os_thread.h>
#include <os_time.h>
#include <core_scoped_ptr.h>
#include <utility_buffer_queue.h>
#include <utility_memory.h>
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
#if 0
    log_verbose("tag", "data addr: %p size: %d\n", data, size);
    log_verbose("tag", "payload data1: %08x\n", *((uint32_t*)data));
    log_verbose("tag", "payload data2: %08x\n", *((uint32_t*)data+1));
    log_verbose("tag", "payload data3: %08x\n", *((uint32_t*)data+2));
    log_verbose("tag", "payload data4: %08x\n", *((uint32_t*)data+3));
#else
    log_verbose("tag", "payload data: %08x\n",
                  *((uint32_t*)data)
                  );
#endif
    return 0;
  }
  std::list<uint16_t> _sequence_numbers;
};

class TestRtpFeedback : public NullRtpFeedback {
public:
  explicit TestRtpFeedback() {}
  virtual ~TestRtpFeedback() {}

  void OnIncomingSSRCChanged(const uint32_t ssrc) override {
    printf("OnIncomingSSRCChanged..\n");
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
    printf("SendRtp...---%08d\n", _count);
    _count++;
    const unsigned char* ptr = static_cast<const unsigned char*>(packet);
    DumpRTPHeader((uint8_t*)ptr);
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

int32_t main(int32_t argc, char *argv[]) {
  int32_t ret = -1;
  bool audio __attribute((__unused__)) = false;
  Clock* clock __attribute((__unused__)) = NULL;
  clock = Clock::GetRealTimeClock();

  /***************************************************************
  *
  *                  RTP sender/receive
  *
  ***************************************************************/
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
  ret = rtp_sender->RegisterPayload("I420", kPayloadType, 9000, 1, 0);
  CHECK_EQ(0, ret);
  scoped_array<uint8_t> payload_data(new uint8_t[65000]);
  uint32_t payload_data_length = 3030;
  for (uint32_t i = 0; i < payload_data_length; ++i) {
    payload_data[i] = i % 128;
  }
  rtp_payload_registry->SetRtxSsrc(kTestSsrc + 1);
  bool created_new_payload = false;
  ret = rtp_payload_registry->RegisterReceivePayload(
                      "I420",
                      kPayloadType,
                      9000,
                      1,
                      0, &created_new_payload);
  CHECK_EQ(0, ret);

  //rtp_sender->SetRtxStatus(rtx_method);
  rtp_sender->SetRtxSsrc(kTestSsrc + 1);
  //transport->DropEveryNthPacket(loss);
  uint32_t timestamp = 3000;
  //uint16_t nack_list[kVideoNackListSize];
  for (int frame = 0; frame < kNumFrames; ++frame) {
    ret = rtp_sender->SendOutgoingData(
                     kVideoFrameDelta, kPayloadType, timestamp,
                     timestamp / 90, payload_data.get(), payload_data_length, NULL, NULL);
    CHECK_EQ(0, ret);
    // Min required delay until retransmit = 5 + RTT ms (RTT = 0).
    //fake_clock.AdvanceTimeMilliseconds(5);
    //int length = BuildNackList(nack_list);
    //if (length > 0)
    //  rtp_rtcp_module_->SendNACK(nack_list, length);
    //fake_clock.AdvanceTimeMilliseconds(28);  //  33ms - 5ms delay.
    //rtp_rtcp_module_->Process();
    // Prepare next frame.
    timestamp += 3000;
  }
  receiver->_sequence_numbers.sort();

  return 0;
}
