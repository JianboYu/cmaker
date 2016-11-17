#include <protocol_rtcp_sender.h>
#include <protocol_rtcp_receiver.h>
#include <protocol_receive_statistics.h>
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

class MockTransport : public Transport,
                      public NullRtpData {
 public:
  MockTransport() {}

  bool SendRtp(const uint8_t* /*data*/,
               size_t /*len*/,
               const PacketOptions& options) override {
    return false;
  }
  bool SendRtcp(const uint8_t* data, size_t len) override {

    log_verbose("tag", "SendRtcp data: %p size: %d\n", data, len);
    const uint8_t* packet = static_cast<const uint8_t*>(data);
    RTCPUtility::RTCPParserV2 parser(packet, len, true);
    CHECK(parser.IsValid());

    return true;
  }
  int OnReceivedPayloadData(const uint8_t* payload_data,
                            const size_t payload_size,
                            const WebRtcRTPHeader* rtp_header) override {
    return 0;
  }
};

const uint32_t kSenderSsrc = 0x11111111;
const uint32_t kRemoteSsrc = 0x22222222;

int32_t main(int32_t argc, char *argv[]) {
  int32_t ret __attribute((__unused__)) = -1;
  bool audio __attribute((__unused__)) = false;
  Clock* clock __attribute((__unused__)) = NULL;
  clock = Clock::GetRealTimeClock();
  RtcEventLog* event_log = NULL;
  scoped_ptr<MockTransport> transport(new MockTransport());

  /***************************************************************
  *
  *                  RTCP sender/receive
  *
  ***************************************************************/
  scoped_ptr<ReceiveStatistics> receive_statistics(ReceiveStatistics::Create(clock));
  scoped_ptr<RtcpPacketTypeCounterObserver>  \
          packet_type_counter_observer(new RtcpPacketTypeCounterObserverImpl()) ;
  scoped_ptr<RTCPSender> rtcp_sender(
                        new RTCPSender(audio,
                                       clock,
                                       receive_statistics.get(),
                                       packet_type_counter_observer.get(),
                                       event_log,
                                       transport.get()));
  rtcp_sender->SetSSRC(kSenderSsrc);
  rtcp_sender->SetRemoteSSRC(kRemoteSsrc);

  const uint32_t kPacketCount = 0x12345;
  const uint32_t kOctetCount = 0x23456;
  rtcp_sender->SetRTCPStatus(RtcpMode::kReducedSize);
  RTCPSender::FeedbackState feedback_state;
  feedback_state.packets_sent = kPacketCount;
  feedback_state.media_bytes_sent = kOctetCount;
  uint32_t ntp_secs;
  uint32_t ntp_frac;
  clock->CurrentNtp(ntp_secs, ntp_frac);
  CHECK_EQ(0, rtcp_sender->SendRTCP(feedback_state, kRtcpSr));
#if 0
  CHECK_EQ(1, parser()->sender_report()->num_packets());
  CHECK_EQ(kSenderSsrc, parser()->sender_report()->Ssrc());
  CHECK_EQ(ntp_secs, parser()->sender_report()->NtpSec());
  CHECK_EQ(ntp_frac, parser()->sender_report()->NtpFrac());
  CHECK_EQ(kPacketCount, parser()->sender_report()->PacketCount());
  CHECK_EQ(kOctetCount, parser()->sender_report()->OctetCount());
  CHECK_EQ(0, parser()->report_block()->num_packets());
#endif

  bool receiver_only = true;
  scoped_ptr<RtcpPacketTypeCounterObserver>  \
        packet_type_counter_observer_rec (new RtcpPacketTypeCounterObserverImpl());
  scoped_ptr<RtcpBandwidthObserver> rtcp_bandwidth_observer;
  scoped_ptr<RtcpIntraFrameObserver> rtcp_intra_frame_observer;
  scoped_ptr<TransportFeedbackObserver> transport_feedback_observer;
  scoped_ptr<RTCPReceiver> rtcp_receiver (
                        new RTCPReceiver(clock,
                                         receiver_only,
                                         packet_type_counter_observer_rec.get(),
                                         rtcp_bandwidth_observer.get(),
                                         rtcp_intra_frame_observer.get(),
                                         transport_feedback_observer.get()));


  return 0;
}
