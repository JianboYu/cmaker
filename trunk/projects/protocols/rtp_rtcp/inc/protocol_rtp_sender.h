/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PROTOCOL_RTP_SENDER_H_
#define PROTOCOL_RTP_SENDER_H_

#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "os_typedefs.h"
#include "os_mutex.h"

#include "core_constructor.h"
#include "core_scoped_ptr.h"

#include "protocol_common_types.h"
#include "protocol_clock.h"
#include "protocol_random.h"
#include "protocol_transport.h"
#include "protocol_rate_statistics.h"
#include "protocol_rtp_rtcp_defines.h"
#include "protocol_rtp_header_extension.h"
#include "protocol_rtp_packet_history.h"
#include "protocol_rtp_rtcp_config.h"
#include "protocol_rtp_utility.h"
#include "protocol_ssrc_database.h"
#include "protocol_event_log.h"
#include "protocol_playout_delay_oracle.h"
#include "protocol_rate_limiter.h"

using namespace os;
using namespace core;
namespace protocol {

class RTPSenderAudio;
class RTPSenderVideo;

class RTPSenderInterface {
 public:
  RTPSenderInterface() {}
  virtual ~RTPSenderInterface() {}

  virtual uint32_t SSRC() const = 0;
  virtual uint32_t Timestamp() const = 0;

  virtual int32_t BuildRTPheader(uint8_t* data_buffer,
                                 int8_t payload_type,
                                 bool marker_bit,
                                 uint32_t capture_timestamp,
                                 int64_t capture_time_ms,
                                 bool timestamp_provided = true,
                                 bool inc_sequence_number = true) = 0;

  // This returns the expected header length taking into consideration
  // the optional RTP header extensions that may not be currently active.
  virtual size_t RtpHeaderLength() const = 0;
  // Returns the next sequence number to use for a packet and allocates
  // 'packets_to_send' number of sequence numbers. It's important all allocated
  // sequence numbers are used in sequence to avoid perceived packet loss.
  virtual uint16_t AllocateSequenceNumber(uint16_t packets_to_send) = 0;
  virtual uint16_t SequenceNumber() const = 0;
  virtual size_t MaxPayloadLength() const = 0;
  virtual size_t MaxDataPayloadLength() const = 0;
  virtual uint16_t ActualSendBitrateKbit() const = 0;

  virtual int32_t SendToNetwork(uint8_t* data_buffer,
                                size_t payload_length,
                                size_t rtp_header_length,
                                int64_t capture_time_ms,
                                StorageType storage,
                                RtpPacketSender::Priority priority) = 0;

  virtual bool UpdateVideoRotation(uint8_t* rtp_packet,
                                   size_t rtp_packet_length,
                                   const RTPHeader& rtp_header,
                                   VideoRotation rotation) const = 0;
  virtual bool IsRtpHeaderExtensionRegistered(RTPExtensionType type) = 0;
  virtual bool ActivateCVORtpHeaderExtension() = 0;
};

class RTPSender : public RTPSenderInterface {
 public:
  RTPSender(bool audio,
            Clock* clock,
            Transport* transport,
            RtpPacketSender* paced_sender,
            TransportSequenceNumberAllocator* sequence_number_allocator,
            TransportFeedbackObserver* transport_feedback_callback,
            BitrateStatisticsObserver* bitrate_callback,
            FrameCountObserver* frame_count_observer,
            SendSideDelayObserver* send_side_delay_observer,
            RtcEventLog* event_log,
            SendPacketObserver* send_packet_observer,
            RateLimiter* nack_rate_limiter);

  virtual ~RTPSender();

  void ProcessBitrate();

  uint16_t ActualSendBitrateKbit() const override;

  uint32_t VideoBitrateSent() const;
  uint32_t FecOverheadRate() const;
  uint32_t NackOverheadRate() const;

  // Includes size of RTP and FEC headers.
  size_t MaxDataPayloadLength() const override;

  int32_t RegisterPayload(const char* payload_name,
                          const int8_t payload_type,
                          const uint32_t frequency,
                          const size_t channels,
                          const uint32_t rate);

  int32_t DeRegisterSendPayload(const int8_t payload_type);

  void SetSendPayloadType(int8_t payload_type);

  int8_t SendPayloadType() const;

  int SendPayloadFrequency() const;

  void SetSendingStatus(bool enabled);

  void SetSendingMediaStatus(bool enabled);
  bool SendingMedia() const;

  void GetDataCounters(StreamDataCounters* rtp_stats,
                       StreamDataCounters* rtx_stats) const;

  uint32_t StartTimestamp() const;
  void SetStartTimestamp(uint32_t timestamp, bool force);

  uint32_t GenerateNewSSRC();
  void SetSSRC(uint32_t ssrc);

  uint16_t SequenceNumber() const override;
  void SetSequenceNumber(uint16_t seq);

  void SetCsrcs(const std::vector<uint32_t>& csrcs);

  void SetMaxPayloadLength(size_t max_payload_length);

  int32_t SendOutgoingData(FrameType frame_type,
                           int8_t payload_type,
                           uint32_t timestamp,
                           int64_t capture_time_ms,
                           const uint8_t* payload_data,
                           size_t payload_size,
                           const RTPFragmentationHeader* fragmentation,
                           const RTPVideoHeader* rtp_hdr = NULL);

  // RTP header extension
  int32_t SetTransmissionTimeOffset(int32_t transmission_time_offset);
  int32_t SetAbsoluteSendTime(uint32_t absolute_send_time);
  void SetVideoRotation(VideoRotation rotation);
  int32_t SetTransportSequenceNumber(uint16_t sequence_number);

  int32_t RegisterRtpHeaderExtension(RTPExtensionType type, uint8_t id);
  bool IsRtpHeaderExtensionRegistered(RTPExtensionType type) override;
  int32_t DeregisterRtpHeaderExtension(RTPExtensionType type);

  size_t RtpHeaderExtensionLength() const;

  uint16_t BuildRTPHeaderExtension(uint8_t* data_buffer, bool marker_bit) const;

  uint8_t BuildTransmissionTimeOffsetExtension(uint8_t *data_buffer) const;
  uint8_t BuildAudioLevelExtension(uint8_t* data_buffer) const;
  uint8_t BuildAbsoluteSendTimeExtension(uint8_t* data_buffer) const;
  uint8_t BuildVideoRotationExtension(uint8_t* data_buffer) const;
  uint8_t BuildTransportSequenceNumberExtension(uint8_t* data_buffer,
                                                uint16_t sequence_number) const;
  uint8_t BuildPlayoutDelayExtension(uint8_t* data_buffer,
                                     uint16_t min_playout_delay_ms,
                                     uint16_t max_playout_delay_ms) const;

  // Verifies that the specified extension is registered, and that it is
  // present in rtp packet. If extension is not registered kNotRegistered is
  // returned. If extension cannot be found in the rtp header, or if it is
  // malformed, kError is returned. Otherwise *extension_offset is set to the
  // offset of the extension from the beginning of the rtp packet and kOk is
  // returned.
  enum class ExtensionStatus {
    kNotRegistered,
    kOk,
    kError,
  };
  ExtensionStatus VerifyExtension(RTPExtensionType extension_type,
                                  uint8_t* rtp_packet,
                                  size_t rtp_packet_length,
                                  const RTPHeader& rtp_header,
                                  size_t extension_length_bytes,
                                  size_t* extension_offset) const;

  bool UpdateAudioLevel(uint8_t* rtp_packet,
                        size_t rtp_packet_length,
                        const RTPHeader& rtp_header,
                        bool is_voiced,
                        uint8_t dBov) const;

  bool UpdateVideoRotation(uint8_t* rtp_packet,
                           size_t rtp_packet_length,
                           const RTPHeader& rtp_header,
                           VideoRotation rotation) const override;

  bool TimeToSendPacket(uint16_t sequence_number,
                        int64_t capture_time_ms,
                        bool retransmission,
                        int probe_cluster_id);
  size_t TimeToSendPadding(size_t bytes, int probe_cluster_id);

  // NACK.
  int SelectiveRetransmissions() const;
  int SetSelectiveRetransmissions(uint8_t settings);
  void OnReceivedNACK(const std::list<uint16_t>& nack_sequence_numbers,
                      int64_t avg_rtt);

  void SetStorePacketsStatus(bool enable, uint16_t number_to_store);

  bool StorePackets() const;

  int32_t ReSendPacket(uint16_t packet_id, int64_t min_resend_time = 0);

  // Feedback to decide when to stop sending playout delay.
  void OnReceivedRtcpReportBlocks(const ReportBlockList& report_blocks);

  // RTX.
  void SetRtxStatus(int mode);
  int RtxStatus() const;

  uint32_t RtxSsrc() const;
  void SetRtxSsrc(uint32_t ssrc);

  void SetRtxPayloadType(int payload_type, int associated_payload_type);

  // Functions wrapping RTPSenderInterface.
  int32_t BuildRTPheader(uint8_t* data_buffer,
                         int8_t payload_type,
                         bool marker_bit,
                         uint32_t capture_timestamp,
                         int64_t capture_time_ms,
                         const bool timestamp_provided = true,
                         const bool inc_sequence_number = true) override;

  size_t RtpHeaderLength() const override;
  uint16_t AllocateSequenceNumber(uint16_t packets_to_send) override;
  size_t MaxPayloadLength() const override;

  // Current timestamp.
  uint32_t Timestamp() const override;
  uint32_t SSRC() const override;

  int32_t SendToNetwork(uint8_t* data_buffer,
                        size_t payload_length,
                        size_t rtp_header_length,
                        int64_t capture_time_ms,
                        StorageType storage,
                        RtpPacketSender::Priority priority) override;

  // Audio.

  // Send a DTMF tone using RFC 2833 (4733).
  int32_t SendTelephoneEvent(uint8_t key, uint16_t time_ms, uint8_t level);

  // Set audio packet size, used to determine when it's time to send a DTMF
  // packet in silence (CNG).
  int32_t SetAudioPacketSize(uint16_t packet_size_samples);

  // Store the audio level in d_bov for
  // header-extension-for-audio-level-indication.
  int32_t SetAudioLevel(uint8_t level_d_bov);

  // Set payload type for Redundant Audio Data RFC 2198.
  int32_t SetRED(int8_t payload_type);

  // Get payload type for Redundant Audio Data RFC 2198.
  int32_t RED(int8_t *payload_type) const;

  RtpVideoCodecTypes VideoCodecType() const;

  uint32_t MaxConfiguredBitrateVideo() const;

  // FEC.
  void SetGenericFECStatus(bool enable,
                           uint8_t payload_type_red,
                           uint8_t payload_type_fec);

  void GenericFECStatus(bool* enable,
                        uint8_t* payload_type_red,
                        uint8_t* payload_type_fec) const;

  int32_t SetFecParameters(const FecProtectionParams *delta_params,
                           const FecProtectionParams *key_params);

  size_t SendPadData(size_t bytes,
                     bool timestamp_provided,
                     uint32_t timestamp,
                     int64_t capture_time_ms);

  size_t SendPadData(size_t bytes,
                     bool timestamp_provided,
                     uint32_t timestamp,
                     int64_t capture_time_ms,
                     int probe_cluster_id);

  // Called on update of RTP statistics.
  void RegisterRtpStatisticsCallback(StreamDataCountersCallback* callback);
  StreamDataCountersCallback* GetRtpStatisticsCallback() const;

  uint32_t BitrateSent() const;

  void SetRtpState(const RtpState& rtp_state);
  RtpState GetRtpState() const;
  void SetRtxRtpState(const RtpState& rtp_state);
  RtpState GetRtxRtpState() const;
  bool ActivateCVORtpHeaderExtension() override;

 protected:
  int32_t CheckPayloadType(int8_t payload_type, RtpVideoCodecTypes* video_type);

 private:
  // Maps capture time in milliseconds to send-side delay in milliseconds.
  // Send-side delay is the difference between transmission time and capture
  // time.
  typedef std::map<int64_t, int> SendDelayMap;

  size_t CreateRtpHeader(uint8_t* header,
                         int8_t payload_type,
                         uint32_t ssrc,
                         bool marker_bit,
                         uint32_t timestamp,
                         uint16_t sequence_number,
                         const std::vector<uint32_t>& csrcs) const;

  bool PrepareAndSendPacket(uint8_t* buffer,
                            size_t length,
                            int64_t capture_time_ms,
                            bool send_over_rtx,
                            bool is_retransmit,
                            int probe_cluster_id);

  // Return the number of bytes sent.  Note that both of these functions may
  // return a larger value that their argument.
  size_t TrySendRedundantPayloads(size_t bytes, int probe_cluster_id);

  void BuildPaddingPacket(uint8_t* packet,
                          size_t header_length,
                          size_t padding_length);

  void BuildRtxPacket(uint8_t* buffer, size_t* length,
                      uint8_t* buffer_rtx);

  bool SendPacketToNetwork(const uint8_t* packet,
                           size_t size,
                           const PacketOptions& options);

  void UpdateDelayStatistics(int64_t capture_time_ms, int64_t now_ms);
  void UpdateOnSendPacket(int packet_id,
                          int64_t capture_time_ms,
                          uint32_t ssrc);

  // Find the byte position of the RTP extension as indicated by |type| in
  // |rtp_packet|. Return false if such extension doesn't exist.
  bool FindHeaderExtensionPosition(RTPExtensionType type,
                                   const uint8_t* rtp_packet,
                                   size_t rtp_packet_length,
                                   const RTPHeader& rtp_header,
                                   size_t* position) const;

  void UpdateTransmissionTimeOffset(uint8_t* rtp_packet,
                                    size_t rtp_packet_length,
                                    const RTPHeader& rtp_header,
                                    int64_t time_diff_ms) const;
  void UpdateAbsoluteSendTime(uint8_t* rtp_packet,
                              size_t rtp_packet_length,
                              const RTPHeader& rtp_header,
                              int64_t now_ms) const;

  bool UpdateTransportSequenceNumber(uint16_t sequence_number,
                                     uint8_t* rtp_packet,
                                     size_t rtp_packet_length,
                                     const RTPHeader& rtp_header) const;

  void UpdatePlayoutDelayLimits(uint8_t* rtp_packet,
                                size_t rtp_packet_length,
                                const RTPHeader& rtp_header,
                                uint16_t min_playout_delay,
                                uint16_t max_playout_delay) const;

  bool AllocateTransportSequenceNumber(int* packet_id) const;

  void UpdateRtpStats(const uint8_t* buffer,
                      size_t packet_length,
                      const RTPHeader& header,
                      bool is_rtx,
                      bool is_retransmit);
  bool IsFecPacket(const uint8_t* buffer, const RTPHeader& header) const;

  Clock* const clock_;
  const int64_t clock_delta_ms_;
  Random random_ GUARDED_BY(send_critsect_);

  const bool audio_configured_;
  const std::unique_ptr<RTPSenderAudio> audio_;
  const std::unique_ptr<RTPSenderVideo> video_;

  RtpPacketSender* const paced_sender_;
  TransportSequenceNumberAllocator* const transport_sequence_number_allocator_;
  TransportFeedbackObserver* const transport_feedback_observer_;
  int64_t last_capture_time_ms_sent_;
  scoped_ptr<Mutex> send_critsect_;

  Transport *transport_;
  bool sending_media_ GUARDED_BY(send_critsect_);

  size_t max_payload_length_;

  int8_t payload_type_ GUARDED_BY(send_critsect_);
  std::map<int8_t, RtpUtility::Payload*> payload_type_map_;

  RtpHeaderExtensionMap rtp_header_extension_map_;
  int32_t transmission_time_offset_;
  uint32_t absolute_send_time_;
  VideoRotation rotation_;
  bool video_rotation_active_;
  uint16_t transport_sequence_number_;

  // Tracks the current request for playout delay limits from application
  // and decides whether the current RTP frame should include the playout
  // delay extension on header.
  PlayoutDelayOracle playout_delay_oracle_;
  bool playout_delay_active_ GUARDED_BY(send_critsect_);

  RTPPacketHistory packet_history_;

  // Statistics
  scoped_ptr<Mutex> statistics_crit_;
  SendDelayMap send_delays_ GUARDED_BY(statistics_crit_);
  FrameCounts frame_counts_ GUARDED_BY(statistics_crit_);
  StreamDataCounters rtp_stats_ GUARDED_BY(statistics_crit_);
  StreamDataCounters rtx_rtp_stats_ GUARDED_BY(statistics_crit_);
  StreamDataCountersCallback* rtp_stats_callback_ GUARDED_BY(statistics_crit_);
  RateStatistics total_bitrate_sent_ GUARDED_BY(statistics_crit_);
  RateStatistics nack_bitrate_sent_ GUARDED_BY(statistics_crit_);
  FrameCountObserver* const frame_count_observer_;
  SendSideDelayObserver* const send_side_delay_observer_;
  RtcEventLog* const event_log_;
  SendPacketObserver* const send_packet_observer_;
  BitrateStatisticsObserver* const bitrate_callback_;

  // RTP variables
  bool start_timestamp_forced_ GUARDED_BY(send_critsect_);
  uint32_t start_timestamp_ GUARDED_BY(send_critsect_);
  SSRCDatabase* const ssrc_db_;
  uint32_t remote_ssrc_ GUARDED_BY(send_critsect_);
  bool sequence_number_forced_ GUARDED_BY(send_critsect_);
  uint16_t sequence_number_ GUARDED_BY(send_critsect_);
  uint16_t sequence_number_rtx_ GUARDED_BY(send_critsect_);
  bool ssrc_forced_ GUARDED_BY(send_critsect_);
  uint32_t ssrc_ GUARDED_BY(send_critsect_);
  uint32_t timestamp_ GUARDED_BY(send_critsect_);
  int64_t capture_time_ms_ GUARDED_BY(send_critsect_);
  int64_t last_timestamp_time_ms_ GUARDED_BY(send_critsect_);
  bool media_has_been_sent_ GUARDED_BY(send_critsect_);
  bool last_packet_marker_bit_ GUARDED_BY(send_critsect_);
  std::vector<uint32_t> csrcs_ GUARDED_BY(send_critsect_);
  int rtx_ GUARDED_BY(send_critsect_);
  uint32_t ssrc_rtx_ GUARDED_BY(send_critsect_);
  // Mapping rtx_payload_type_map_[associated] = rtx.
  std::map<int8_t, int8_t> rtx_payload_type_map_ GUARDED_BY(send_critsect_);

  RateLimiter* const retransmission_rate_limiter_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RTPSender);
};

}  // namespace protocol

#endif  // PROTOCOL_RTP_SENDER_H_