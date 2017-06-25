/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <stdlib.h>  // srand
#include <algorithm>
#include <utility>

#include <os_log.h>
#include <os_assert.h>

#if 0
#include "system_wrappers/interface/logging.h"
#include "system_wrappers/interface/timeutils.h"
#include "system_wrappers/interface/common_types.h"
#include "rtp_rtcp/source/byte_io.h"
#include "rtp_rtcp/source/playout_delay_oracle.h"
#endif

#include "protocol_rate_limiter.h"
#include "protocol_event_log.h"
#include "protocol_rtp_sender.h"
#include "protocol_byte_io.h"
#include "protocol_timeutils.h"
#include "protocol_rtp_sender_audio.h"
#include "protocol_rtp_sender_video.h"
#include "protocol_rtp_cvo.h"
#include "protocol_time_util.h"

namespace protocol {

// Max in the RFC 3550 is 255 bytes, we limit it to be modulus 32 for SRTP.
static const size_t kMaxPaddingLength = 224;
static const int kSendSideDelayWindowMs = 1000;
static const uint32_t kAbsSendTimeFraction = 18;
static const int kBitrateStatisticsWindowMs = 1000;

namespace {

const size_t kRtpHeaderLength = 12;
const uint16_t kMaxInitRtpSeqNumber = 32767;  // 2^15 -1.

const char* FrameTypeToString(FrameType frame_type) {
  switch (frame_type) {
    case kEmptyFrame:
      return "empty";
    case kAudioFrameSpeech: return "audio_speech";
    case kAudioFrameCN: return "audio_cn";
    case kVideoFrameKey: return "video_key";
    case kVideoFrameDelta: return "video_delta";
  }
  return "";
}

// TODO(holmer): Merge this with the implementation in
// remote_bitrate_estimator_abs_send_time.cc.
uint32_t ConvertMsTo24Bits(int64_t time_ms) {
  uint32_t time_24_bits =
      static_cast<uint32_t>(
          ((static_cast<uint64_t>(time_ms) << kAbsSendTimeFraction) + 500) /
          1000) &
      0x00FFFFFF;
  return time_24_bits;
}
}  // namespace

RTPSender::RTPSender(
    bool audio,
    Clock* clock,
    Transport* transport,
    RtpPacketSender* paced_sender,
    TransportSequenceNumberAllocator* sequence_number_allocator,
    TransportFeedbackObserver* transport_feedback_observer,
    BitrateStatisticsObserver* bitrate_callback,
    FrameCountObserver* frame_count_observer,
    SendSideDelayObserver* send_side_delay_observer,
    RtcEventLog* event_log,
    SendPacketObserver* send_packet_observer,
    RateLimiter* retransmission_rate_limiter)
    : clock_(clock),
      // TODO(holmer): Remove this conversion?
      clock_delta_ms_(clock_->TimeInMilliseconds() - TimeMillis()),
      random_(clock_->TimeInMicroseconds()),
      audio_configured_(audio),
      audio_(audio ? new RTPSenderAudio(clock, this) : nullptr),
      video_(audio ? nullptr : new RTPSenderVideo(clock, this)),
      paced_sender_(paced_sender),
      transport_sequence_number_allocator_(sequence_number_allocator),
      transport_feedback_observer_(transport_feedback_observer),
      last_capture_time_ms_sent_(0),
      transport_(transport),
      sending_media_(true),                      // Default to sending media.
      max_payload_length_(IP_PACKET_SIZE - 28),  // Default is IP-v4/UDP.
      payload_type_(-1),
      payload_type_map_(),
      rtp_header_extension_map_(),
      transmission_time_offset_(0),
      absolute_send_time_(0),
      rotation_(kVideoRotation_0),
      video_rotation_active_(false),
      transport_sequence_number_(0),
      playout_delay_active_(false),
      packet_history_(clock),
      // Statistics
      rtp_stats_callback_(nullptr),
      total_bitrate_sent_(kBitrateStatisticsWindowMs,
                          RateStatistics::kBpsScale),
      nack_bitrate_sent_(kBitrateStatisticsWindowMs, RateStatistics::kBpsScale),
      frame_count_observer_(frame_count_observer),
      send_side_delay_observer_(send_side_delay_observer),
      event_log_(event_log),
      send_packet_observer_(send_packet_observer),
      bitrate_callback_(bitrate_callback),
      // RTP variables
      start_timestamp_forced_(false),
      start_timestamp_(0),
      ssrc_db_(SSRCDatabase::GetSSRCDatabase()),
      remote_ssrc_(0),
      sequence_number_forced_(false),
      ssrc_forced_(false),
      timestamp_(0),
      capture_time_ms_(0),
      last_timestamp_time_ms_(0),
      media_has_been_sent_(false),
      last_packet_marker_bit_(false),
      csrcs_(),
      rtx_(kRtxOff),
      retransmission_rate_limiter_(retransmission_rate_limiter) {
  // We need to seed the random generator for BuildPaddingPacket() below.
  // TODO(holmer,tommi): Note that TimeInMilliseconds might return 0 on Mac
  // early on in the process.
  srand(static_cast<uint32_t>(clock_->TimeInMilliseconds()));
  ssrc_ = ssrc_db_->CreateSSRC();
  CHECK(ssrc_ != 0);
  ssrc_rtx_ = ssrc_db_->CreateSSRC();
  CHECK(ssrc_rtx_ != 0);

  // Random start, 16 bits. Can't be 0.
  sequence_number_rtx_ = random_.Rand(1, kMaxInitRtpSeqNumber);
  sequence_number_ = random_.Rand(1, kMaxInitRtpSeqNumber);

  send_critsect_.reset(Mutex::Create());
  statistics_crit_.reset(Mutex::Create());
}

RTPSender::~RTPSender() {
  // TODO(tommi): Use a thread checker to ensure the object is created and
  // deleted on the same thread.  At the moment this isn't possible due to
  // voe::ChannelOwner in voice engine.  To reproduce, run:
  // voe_auto_test --automated --gtest_filter=*MixManyChannelsForStressOpus

  // TODO(tommi,holmer): We don't grab locks in the dtor before accessing member
  // variables but we grab them in all other methods. (what's the design?)
  // Start documenting what thread we're on in what method so that it's easier
  // to understand performance attributes and possibly remove locks.
  if (remote_ssrc_ != 0) {
    ssrc_db_->ReturnSSRC(remote_ssrc_);
  }
  ssrc_db_->ReturnSSRC(ssrc_);

  SSRCDatabase::ReturnSSRCDatabase();
  while (!payload_type_map_.empty()) {
    std::map<int8_t, RtpUtility::Payload*>::iterator it =
        payload_type_map_.begin();
    delete it->second;
    payload_type_map_.erase(it);
  }
}

uint16_t RTPSender::ActualSendBitrateKbit() const {
  AutoLock cs(statistics_crit_.get());
  return static_cast<uint16_t>(
      total_bitrate_sent_.Rate(clock_->TimeInMilliseconds()).value_or(0) /
      1000);
}

uint32_t RTPSender::VideoBitrateSent() const {
  if (video_) {
    return video_->VideoBitrateSent();
  }
  return 0;
}

uint32_t RTPSender::FecOverheadRate() const {
  if (video_) {
    return video_->FecOverheadRate();
  }
  return 0;
}

uint32_t RTPSender::NackOverheadRate() const {
  AutoLock cs(statistics_crit_.get());
  return nack_bitrate_sent_.Rate(clock_->TimeInMilliseconds()).value_or(0);
}

int32_t RTPSender::SetTransmissionTimeOffset(int32_t transmission_time_offset) {
  if (transmission_time_offset > (0x800000 - 1) ||
      transmission_time_offset < -(0x800000 - 1)) {  // Word24.
    return -1;
  }
  AutoLock cs(send_critsect_.get());
  transmission_time_offset_ = transmission_time_offset;
  return 0;
}

int32_t RTPSender::SetAbsoluteSendTime(uint32_t absolute_send_time) {
  if (absolute_send_time > 0xffffff) {  // UWord24.
    return -1;
  }
  AutoLock cs(send_critsect_.get());
  absolute_send_time_ = absolute_send_time;
  return 0;
}

void RTPSender::SetVideoRotation(VideoRotation rotation) {
  AutoLock cs(send_critsect_.get());
  rotation_ = rotation;
}

int32_t RTPSender::SetTransportSequenceNumber(uint16_t sequence_number) {
  AutoLock cs(send_critsect_.get());
  transport_sequence_number_ = sequence_number;
  return 0;
}

int32_t RTPSender::RegisterRtpHeaderExtension(RTPExtensionType type,
                                              uint8_t id) {
  AutoLock cs(send_critsect_.get());
  switch (type) {
    case kRtpExtensionVideoRotation:
      video_rotation_active_ = false;
      return rtp_header_extension_map_.RegisterInactive(type, id);
    case kRtpExtensionPlayoutDelay:
      playout_delay_active_ = false;
      return rtp_header_extension_map_.RegisterInactive(type, id);
    case kRtpExtensionTransmissionTimeOffset:
    case kRtpExtensionAbsoluteSendTime:
    case kRtpExtensionAudioLevel:
    case kRtpExtensionTransportSequenceNumber:
      return rtp_header_extension_map_.Register(type, id);
    case kRtpExtensionNone:
    case kRtpExtensionNumberOfExtensions:
      LOG(LS_ERROR) << "Invalid RTP extension type for registration";
      return -1;
  }
  return -1;
}

bool RTPSender::IsRtpHeaderExtensionRegistered(RTPExtensionType type) {
  AutoLock cs(send_critsect_.get());
  return rtp_header_extension_map_.IsRegistered(type);
}

int32_t RTPSender::DeregisterRtpHeaderExtension(RTPExtensionType type) {
  AutoLock cs(send_critsect_.get());
  return rtp_header_extension_map_.Deregister(type);
}

size_t RTPSender::RtpHeaderExtensionLength() const {
  AutoLock cs(send_critsect_.get());
  return rtp_header_extension_map_.GetTotalLengthInBytes();
}

int32_t RTPSender::RegisterPayload(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    int8_t payload_number,
    uint32_t frequency,
    size_t channels,
    uint32_t rate) {
  CHECK_LT(strlen(payload_name), RTP_PAYLOAD_NAME_SIZE);
  AutoLock cs(send_critsect_.get());

  std::map<int8_t, RtpUtility::Payload*>::iterator it =
      payload_type_map_.find(payload_number);

  if (payload_type_map_.end() != it) {
    // We already use this payload type.
    RtpUtility::Payload* payload = it->second;
    assert(payload);

    // Check if it's the same as we already have.
    if (RtpUtility::StringCompare(
            payload->name, payload_name, RTP_PAYLOAD_NAME_SIZE - 1)) {
      if (audio_configured_ && payload->audio &&
          payload->typeSpecific.Audio.frequency == frequency &&
          (payload->typeSpecific.Audio.rate == rate ||
           payload->typeSpecific.Audio.rate == 0 || rate == 0)) {
        payload->typeSpecific.Audio.rate = rate;
        // Ensure that we update the rate if new or old is zero.
        return 0;
      }
      if (!audio_configured_ && !payload->audio) {
        return 0;
      }
    }
    return -1;
  }
  int32_t ret_val = 0;
  RtpUtility::Payload* payload = nullptr;
  if (audio_configured_) {
    // TODO(mflodman): Change to CreateAudioPayload and make static.
    ret_val = audio_->RegisterAudioPayload(payload_name, payload_number,
                                           frequency, channels, rate, &payload);
  } else {
    payload = video_->CreateVideoPayload(payload_name, payload_number);
  }
  if (payload) {
    payload_type_map_[payload_number] = payload;
  }
  return ret_val;
}

int32_t RTPSender::DeRegisterSendPayload(int8_t payload_type) {
  AutoLock cs(send_critsect_.get());

  std::map<int8_t, RtpUtility::Payload*>::iterator it =
      payload_type_map_.find(payload_type);

  if (payload_type_map_.end() == it) {
    return -1;
  }
  RtpUtility::Payload* payload = it->second;
  delete payload;
  payload_type_map_.erase(it);
  return 0;
}

void RTPSender::SetSendPayloadType(int8_t payload_type) {
  AutoLock cs(send_critsect_.get());
  payload_type_ = payload_type;
}

int8_t RTPSender::SendPayloadType() const {
  AutoLock cs(send_critsect_.get());
  return payload_type_;
}

int RTPSender::SendPayloadFrequency() const {
  return audio_ != NULL ? audio_->AudioFrequency() : kVideoPayloadTypeFrequency;
}

void RTPSender::SetMaxPayloadLength(size_t max_payload_length) {
  // Sanity check.
  /*CHECK(max_payload_length >= 100 && max_payload_length <= IP_PACKET_SIZE)
      << "Invalid max payload length: " << max_payload_length;*/
  AutoLock cs(send_critsect_.get());
  max_payload_length_ = max_payload_length;
}

size_t RTPSender::MaxDataPayloadLength() const {
  int rtx;
  {
    AutoLock cs(send_critsect_.get());
    rtx = rtx_;
  }
  if (audio_configured_) {
    return max_payload_length_ - RtpHeaderLength();
  } else {
    return max_payload_length_ - RtpHeaderLength()  // RTP overhead.
           - video_->FECPacketOverhead()            // FEC/ULP/RED overhead.
           - ((rtx) ? 2 : 0);                       // RTX overhead.
  }
}

size_t RTPSender::MaxPayloadLength() const {
  return max_payload_length_;
}

void RTPSender::SetRtxStatus(int mode) {
  AutoLock cs(send_critsect_.get());
  rtx_ = mode;
}

int RTPSender::RtxStatus() const {
  AutoLock cs(send_critsect_.get());
  return rtx_;
}

void RTPSender::SetRtxSsrc(uint32_t ssrc) {
  AutoLock cs(send_critsect_.get());
  ssrc_rtx_ = ssrc;
}

uint32_t RTPSender::RtxSsrc() const {
  AutoLock cs(send_critsect_.get());
  return ssrc_rtx_;
}

void RTPSender::SetRtxPayloadType(int payload_type,
                                  int associated_payload_type) {
  AutoLock cs(send_critsect_.get());
  CHECK_LE(payload_type, 127);
  CHECK_LE(associated_payload_type, 127);
  if (payload_type < 0) {
    LOG(LS_ERROR) << "Invalid RTX payload type: " << payload_type;
    return;
  }

  rtx_payload_type_map_[associated_payload_type] = payload_type;
}

int32_t RTPSender::CheckPayloadType(int8_t payload_type,
                                    RtpVideoCodecTypes* video_type) {
  AutoLock cs(send_critsect_.get());

  if (payload_type < 0) {
    LOG(LS_ERROR) << "Invalid payload_type " << payload_type;
    return -1;
  }
  if (audio_configured_) {
    int8_t red_pl_type = -1;
    if (audio_->RED(&red_pl_type) == 0) {
      // We have configured RED.
      if (red_pl_type == payload_type) {
        // And it's a match...
        return 0;
      }
    }
  }
  if (payload_type_ == payload_type) {
    if (!audio_configured_) {
      *video_type = video_->VideoCodecType();
    }
    return 0;
  }
  std::map<int8_t, RtpUtility::Payload*>::iterator it =
      payload_type_map_.find(payload_type);
  if (it == payload_type_map_.end()) {
    LOG(LS_WARNING) << "Payload type " << static_cast<int>(payload_type)
                    << " not registered.";
    return -1;
  }
  SetSendPayloadType(payload_type);
  RtpUtility::Payload* payload = it->second;
  assert(payload);
  if (!payload->audio && !audio_configured_) {
    video_->SetVideoCodecType(payload->typeSpecific.Video.videoCodecType);
    *video_type = payload->typeSpecific.Video.videoCodecType;
  }
  return 0;
}

bool RTPSender::ActivateCVORtpHeaderExtension() {
  if (!video_rotation_active_) {
    AutoLock cs(send_critsect_.get());
    if (rtp_header_extension_map_.SetActive(kRtpExtensionVideoRotation, true)) {
      video_rotation_active_ = true;
    }
  }
  return video_rotation_active_;
}

int32_t RTPSender::SendOutgoingData(FrameType frame_type,
                                    int8_t payload_type,
                                    uint32_t capture_timestamp,
                                    int64_t capture_time_ms,
                                    const uint8_t* payload_data,
                                    size_t payload_size,
                                    const RTPFragmentationHeader* fragmentation,
                                    const RTPVideoHeader* rtp_hdr) {
  uint32_t ssrc;
  uint16_t sequence_number;
  {
    // Drop this packet if we're not sending media packets.
    AutoLock cs(send_critsect_.get());
    ssrc = ssrc_;
    sequence_number = sequence_number_;
    if (!sending_media_) {
      return 0;
    }
  }
  RtpVideoCodecTypes video_type = kRtpVideoGeneric;
  if (CheckPayloadType(payload_type, &video_type) != 0) {
    LOG(LS_ERROR) << "Don't send data with unknown payload type: "
                  << static_cast<int>(payload_type) << ".";
    return -1;
  }

  int32_t ret_val;
  if (audio_configured_) {
    log_verbose("Audio", "Send capture timestamp[%d] type[%s]\n",capture_timestamp,
                 FrameTypeToString(frame_type));
    assert(frame_type == kAudioFrameSpeech || frame_type == kAudioFrameCN ||
           frame_type == kEmptyFrame);

    ret_val = audio_->SendAudio(frame_type, payload_type, capture_timestamp,
                                payload_data, payload_size, fragmentation);
  } else {
    log_verbose("Video", "Send capture timestamp[%d] type[%s]\n",capture_timestamp,
                 FrameTypeToString(frame_type));
    assert(frame_type != kAudioFrameSpeech && frame_type != kAudioFrameCN);

    if (frame_type == kEmptyFrame)
      return 0;

    if (rtp_hdr) {
      playout_delay_oracle_.UpdateRequest(ssrc, rtp_hdr->playout_delay,
                                          sequence_number);
    }

    // Update the active/inactive status of playout delay extension based
    // on what the oracle indicates.
    {
      AutoLock cs(send_critsect_.get());
      if (playout_delay_active_ != playout_delay_oracle_.send_playout_delay()) {
        playout_delay_active_ = playout_delay_oracle_.send_playout_delay();
        rtp_header_extension_map_.SetActive(kRtpExtensionPlayoutDelay,
                                            playout_delay_active_);
      }
    }

    ret_val = video_->SendVideo(
        video_type, frame_type, payload_type, capture_timestamp,
        capture_time_ms, payload_data, payload_size, fragmentation, rtp_hdr);
  }

  AutoLock cs(statistics_crit_.get());
  // Note: This is currently only counting for video.
  if (frame_type == kVideoFrameKey) {
    ++frame_counts_.key_frames;
  } else if (frame_type == kVideoFrameDelta) {
    ++frame_counts_.delta_frames;
  }
  if (frame_count_observer_) {
    frame_count_observer_->FrameCountUpdated(frame_counts_, ssrc);
  }

  return ret_val;
}

size_t RTPSender::TrySendRedundantPayloads(size_t bytes_to_send,
                                           int probe_cluster_id) {
  {
    AutoLock cs(send_critsect_.get());
    if (!sending_media_)
      return 0;
    if ((rtx_ & kRtxRedundantPayloads) == 0)
      return 0;
  }

  uint8_t buffer[IP_PACKET_SIZE];
  int bytes_left = static_cast<int>(bytes_to_send);
  while (bytes_left > 0) {
    size_t length = bytes_left;
    int64_t capture_time_ms;
    if (!packet_history_.GetBestFittingPacket(buffer, &length,
                                              &capture_time_ms)) {
      break;
    }
    if (!PrepareAndSendPacket(buffer, length, capture_time_ms, true, false,
                              probe_cluster_id))
      break;
    RtpUtility::RtpHeaderParser rtp_parser(buffer, length);
    RTPHeader rtp_header;
    rtp_parser.Parse(&rtp_header);
    bytes_left -= static_cast<int>(length - rtp_header.headerLength);
  }
  return bytes_to_send - bytes_left;
}

void RTPSender::BuildPaddingPacket(uint8_t* packet,
                                   size_t header_length,
                                   size_t padding_length) {
  packet[0] |= 0x20;  // Set padding bit.
  int32_t* data = reinterpret_cast<int32_t*>(&(packet[header_length]));

  // Fill data buffer with random data.
  for (size_t j = 0; j < (padding_length >> 2); ++j) {
    data[j] = rand();  // NOLINT
  }
  // Set number of padding bytes in the last byte of the packet.
  packet[header_length + padding_length - 1] =
      static_cast<uint8_t>(padding_length);
}

size_t RTPSender::SendPadData(size_t bytes,
                              bool timestamp_provided,
                              uint32_t timestamp,
                              int64_t capture_time_ms) {
  return SendPadData(bytes, timestamp_provided, timestamp, capture_time_ms,
                     PacketInfo::kNotAProbe);
}

size_t RTPSender::SendPadData(size_t bytes,
                              bool timestamp_provided,
                              uint32_t timestamp,
                              int64_t capture_time_ms,
                              int probe_cluster_id) {
  // Always send full padding packets. This is accounted for by the
  // RtpPacketSender,
  // which will make sure we don't send too much padding even if a single packet
  // is larger than requested.
  size_t padding_bytes_in_packet =
      std::min(MaxDataPayloadLength(), kMaxPaddingLength);
  size_t bytes_sent = 0;
  bool using_transport_seq = rtp_header_extension_map_.IsRegistered(
                                 kRtpExtensionTransportSequenceNumber) &&
                             transport_sequence_number_allocator_;
  for (; bytes > 0; bytes -= padding_bytes_in_packet) {
    if (bytes < padding_bytes_in_packet)
      bytes = padding_bytes_in_packet;

    uint32_t ssrc;
    uint16_t sequence_number;
    int payload_type;
    bool over_rtx;
    {
      AutoLock cs(send_critsect_.get());
      if (!sending_media_)
        return bytes_sent;
      if (!timestamp_provided) {
        timestamp = timestamp_;
        capture_time_ms = capture_time_ms_;
      }
      if (rtx_ == kRtxOff) {
        // Without RTX we can't send padding in the middle of frames.
        if (!last_packet_marker_bit_)
          return 0;
        ssrc = ssrc_;
        sequence_number = sequence_number_;
        ++sequence_number_;
        payload_type = payload_type_;
        over_rtx = false;
      } else {
        // Without abs-send-time or transport sequence number a media packet
        // must be sent before padding so that the timestamps used for
        // estimation are correct.
        if (!media_has_been_sent_ &&
            !(rtp_header_extension_map_.IsRegistered(
                  kRtpExtensionAbsoluteSendTime) ||
              using_transport_seq)) {
          return 0;
        }
        // Only change change the timestamp of padding packets sent over RTX.
        // Padding only packets over RTP has to be sent as part of a media
        // frame (and therefore the same timestamp).
        if (last_timestamp_time_ms_ > 0) {
          timestamp +=
              (clock_->TimeInMilliseconds() - last_timestamp_time_ms_) * 90;
          capture_time_ms +=
              (clock_->TimeInMilliseconds() - last_timestamp_time_ms_);
        }
        ssrc = ssrc_rtx_;
        sequence_number = sequence_number_rtx_;
        ++sequence_number_rtx_;
        payload_type = rtx_payload_type_map_.begin()->second;
        over_rtx = true;
      }
    }

    uint8_t padding_packet[IP_PACKET_SIZE];
    size_t header_length =
        CreateRtpHeader(padding_packet, payload_type, ssrc, false, timestamp,
                        sequence_number, std::vector<uint32_t>());
    BuildPaddingPacket(padding_packet, header_length, padding_bytes_in_packet);
    size_t length = padding_bytes_in_packet + header_length;
    int64_t now_ms = clock_->TimeInMilliseconds();

    RtpUtility::RtpHeaderParser rtp_parser(padding_packet, length);
    RTPHeader rtp_header;
    rtp_parser.Parse(&rtp_header);

    if (capture_time_ms > 0) {
      UpdateTransmissionTimeOffset(
          padding_packet, length, rtp_header, now_ms - capture_time_ms);
    }

    UpdateAbsoluteSendTime(padding_packet, length, rtp_header, now_ms);

    PacketOptions options;
    if (AllocateTransportSequenceNumber(&options.packet_id)) {
      if (UpdateTransportSequenceNumber(options.packet_id, padding_packet,
                                        length, rtp_header)) {
        if (transport_feedback_observer_)
          transport_feedback_observer_->AddPacket(options.packet_id, length,
                                                  probe_cluster_id);
      }
    }

    if (!SendPacketToNetwork(padding_packet, length, options))
      break;

    bytes_sent += padding_bytes_in_packet;
    UpdateRtpStats(padding_packet, length, rtp_header, over_rtx, false);
  }

  return bytes_sent;
}

void RTPSender::SetStorePacketsStatus(bool enable, uint16_t number_to_store) {
  packet_history_.SetStorePacketsStatus(enable, number_to_store);
}

bool RTPSender::StorePackets() const {
  return packet_history_.StorePackets();
}

int32_t RTPSender::ReSendPacket(uint16_t packet_id, int64_t min_resend_time) {
  size_t length = IP_PACKET_SIZE;
  uint8_t data_buffer[IP_PACKET_SIZE];
  int64_t capture_time_ms;

  if (!packet_history_.GetPacketAndSetSendTime(packet_id, min_resend_time, true,
                                               data_buffer, &length,
                                               &capture_time_ms)) {
    // Packet not found.
    return 0;
  }

  // Check if we're overusing retransmission bitrate.
  // TODO(sprang): Add histograms for nack success or failure reasons.
  CHECK(retransmission_rate_limiter_);
  if (!retransmission_rate_limiter_->TryUseRate(length))
    return -1;

  if (paced_sender_) {
    RtpUtility::RtpHeaderParser rtp_parser(data_buffer, length);
    RTPHeader header;
    if (!rtp_parser.Parse(&header)) {
      assert(false);
      return -1;
    }
    // Convert from TickTime to Clock since capture_time_ms is based on
    // TickTime.
    int64_t corrected_capture_tims_ms = capture_time_ms + clock_delta_ms_;
    paced_sender_->InsertPacket(
        RtpPacketSender::kNormalPriority, header.ssrc, header.sequenceNumber,
        corrected_capture_tims_ms, length - header.headerLength, true);

    return length;
  }
  int rtx = kRtxOff;
  {
    AutoLock cs(send_critsect_.get());
    rtx = rtx_;
  }
  if (!PrepareAndSendPacket(data_buffer, length, capture_time_ms,
                            (rtx & kRtxRetransmitted) > 0, true,
                            PacketInfo::kNotAProbe)) {
    return -1;
  }
  return static_cast<int32_t>(length);
}

bool RTPSender::SendPacketToNetwork(const uint8_t* packet,
                                    size_t size,
                                    const PacketOptions& options) {
  int bytes_sent = -1;
  if (transport_) {
    bytes_sent = transport_->SendRtp(packet, size, options)
                     ? static_cast<int>(size)
                     : -1;
    if (event_log_ && bytes_sent > 0) {
      //event_log_->LogRtpHeader(kOutgoingPacket, MediaType::ANY, packet, size);
    }
  }
  logi("RTPSender::SendPacketToNetwork size: %d sent: %d\n", size, bytes_sent);
  // TODO(pwestin): Add a separate bitrate for sent bitrate after pacer.
  if (bytes_sent <= 0) {
    LOG(LS_WARNING) << "Transport failed to send packet";
    return false;
  }
  return true;
}

int RTPSender::SelectiveRetransmissions() const {
  if (!video_)
    return -1;
  return video_->SelectiveRetransmissions();
}

int RTPSender::SetSelectiveRetransmissions(uint8_t settings) {
  if (!video_)
    return -1;
  video_->SetSelectiveRetransmissions(settings);
  return 0;
}

void RTPSender::OnReceivedNACK(const std::list<uint16_t>& nack_sequence_numbers,
                               int64_t avg_rtt) {
  logi("RTPSender::OnReceivedNACK num_seqnum: %d avg_rtt: %d\n", nack_sequence_numbers.size(), avg_rtt);
  for (uint16_t seq_no : nack_sequence_numbers) {
    const int32_t bytes_sent = ReSendPacket(seq_no, 5 + avg_rtt);
    if (bytes_sent < 0) {
      // Failed to send one Sequence number. Give up the rest in this nack.
      logw("Failed resending RTP packet:%d Discard rest of packets\n", seq_no);
      break;
    }
  }
}

void RTPSender::OnReceivedRtcpReportBlocks(
    const ReportBlockList& report_blocks) {
  playout_delay_oracle_.OnReceivedRtcpReportBlocks(report_blocks);
}

// Called from pacer when we can send the packet.
bool RTPSender::TimeToSendPacket(uint16_t sequence_number,
                                 int64_t capture_time_ms,
                                 bool retransmission,
                                 int probe_cluster_id) {
  size_t length = IP_PACKET_SIZE;
  uint8_t data_buffer[IP_PACKET_SIZE];
  int64_t stored_time_ms;

  if (!packet_history_.GetPacketAndSetSendTime(sequence_number,
                                               0,
                                               retransmission,
                                               data_buffer,
                                               &length,
                                               &stored_time_ms)) {
    // Packet cannot be found. Allow sending to continue.
    return true;
  }

  int rtx;
  {
    AutoLock cs(send_critsect_.get());
    rtx = rtx_;
  }
  return PrepareAndSendPacket(data_buffer, length, capture_time_ms,
                              retransmission && (rtx & kRtxRetransmitted) > 0,
                              retransmission, probe_cluster_id);
}

bool RTPSender::PrepareAndSendPacket(uint8_t* buffer,
                                     size_t length,
                                     int64_t capture_time_ms,
                                     bool send_over_rtx,
                                     bool is_retransmit,
                                     int probe_cluster_id) {
  uint8_t* buffer_to_send_ptr = buffer;

  RtpUtility::RtpHeaderParser rtp_parser(buffer, length);
  RTPHeader rtp_header;
  rtp_parser.Parse(&rtp_header);
  if (!is_retransmit && rtp_header.markerBit) {
    logv("PacedSend ts: %d\n", capture_time_ms);
  }

  logi("PrepareAndSendPacket timestamp %d seqnum %d\n",
        rtp_header.timestamp, rtp_header.sequenceNumber);

  uint8_t data_buffer_rtx[IP_PACKET_SIZE];
  if (send_over_rtx) {
    BuildRtxPacket(buffer, &length, data_buffer_rtx);
    buffer_to_send_ptr = data_buffer_rtx;
  }

  int64_t now_ms = clock_->TimeInMilliseconds();
  int64_t diff_ms = now_ms - capture_time_ms;
  UpdateTransmissionTimeOffset(buffer_to_send_ptr, length, rtp_header,
                               diff_ms);
  UpdateAbsoluteSendTime(buffer_to_send_ptr, length, rtp_header, now_ms);

  PacketOptions options;
  if (AllocateTransportSequenceNumber(&options.packet_id)) {
    if (UpdateTransportSequenceNumber(options.packet_id, buffer_to_send_ptr,
                                      length, rtp_header)) {
      if (transport_feedback_observer_)
        transport_feedback_observer_->AddPacket(options.packet_id, length,
                                                probe_cluster_id);
    }
  }

  if (!is_retransmit && !send_over_rtx) {
    UpdateDelayStatistics(capture_time_ms, now_ms);
    UpdateOnSendPacket(options.packet_id, capture_time_ms, rtp_header.ssrc);
  }

  bool ret = SendPacketToNetwork(buffer_to_send_ptr, length, options);
  if (ret) {
    AutoLock cs(send_critsect_.get());
    media_has_been_sent_ = true;
  }
  UpdateRtpStats(buffer_to_send_ptr, length, rtp_header, send_over_rtx,
                 is_retransmit);
  return ret;
}

void RTPSender::UpdateRtpStats(const uint8_t* buffer,
                               size_t packet_length,
                               const RTPHeader& header,
                               bool is_rtx,
                               bool is_retransmit) {
  StreamDataCounters* counters;
  // Get ssrc before taking statistics_crit_ to avoid possible deadlock.
  uint32_t ssrc = is_rtx ? RtxSsrc() : SSRC();
  int64_t now_ms = clock_->TimeInMilliseconds();

  AutoLock cs(statistics_crit_.get());
  if (is_rtx) {
    counters = &rtx_rtp_stats_;
  } else {
    counters = &rtp_stats_;
  }

  total_bitrate_sent_.Update(packet_length, now_ms);

  if (counters->first_packet_time_ms == -1)
    counters->first_packet_time_ms = clock_->TimeInMilliseconds();

  if (IsFecPacket(buffer, header))
    counters->fec.AddPacket(packet_length, header);

  if (is_retransmit) {
    counters->retransmitted.AddPacket(packet_length, header);
    nack_bitrate_sent_.Update(packet_length, now_ms);
  }

  counters->transmitted.AddPacket(packet_length, header);

  if (rtp_stats_callback_)
    rtp_stats_callback_->DataCountersUpdated(*counters, ssrc);
}

bool RTPSender::IsFecPacket(const uint8_t* buffer,
                            const RTPHeader& header) const {
  if (!video_) {
    return false;
  }
  bool fec_enabled;
  uint8_t pt_red;
  uint8_t pt_fec;
  video_->GenericFECStatus(&fec_enabled, &pt_red, &pt_fec);
  return fec_enabled &&
      header.payloadType == pt_red &&
      buffer[header.headerLength] == pt_fec;
}

size_t RTPSender::TimeToSendPadding(size_t bytes, int probe_cluster_id) {
  if (audio_configured_ || bytes == 0)
    return 0;
  size_t bytes_sent = TrySendRedundantPayloads(bytes, probe_cluster_id);
  if (bytes_sent < bytes)
    bytes_sent +=
        SendPadData(bytes - bytes_sent, false, 0, 0, probe_cluster_id);
  return bytes_sent;
}

// TODO(pwestin): send in the RtpHeaderParser to avoid parsing it again.
int32_t RTPSender::SendToNetwork(uint8_t* buffer,
                                 size_t payload_length,
                                 size_t rtp_header_length,
                                 int64_t capture_time_ms,
                                 StorageType storage,
                                 RtpPacketSender::Priority priority) {
  size_t length = payload_length + rtp_header_length;
  RtpUtility::RtpHeaderParser rtp_parser(buffer, length);

  RTPHeader rtp_header;
  rtp_parser.Parse(&rtp_header);

  int64_t now_ms = clock_->TimeInMilliseconds();

  // |capture_time_ms| <= 0 is considered invalid.
  // TODO(holmer): This should be changed all over Video Engine so that negative
  // time is consider invalid, while 0 is considered a valid time.
  if (capture_time_ms > 0) {
    UpdateTransmissionTimeOffset(buffer, length, rtp_header,
                                 now_ms - capture_time_ms);
  }

  UpdateAbsoluteSendTime(buffer, length, rtp_header, now_ms);

  // Used for NACK and to spread out the transmission of packets.
  if (packet_history_.PutRTPPacket(buffer, length, capture_time_ms, storage) !=
      0) {
    return -1;
  }

  if (paced_sender_) {
    // Correct offset between implementations of millisecond time stamps in
    // TickTime and Clock.
    int64_t corrected_time_ms = capture_time_ms + clock_delta_ms_;
    paced_sender_->InsertPacket(priority, rtp_header.ssrc,
                                rtp_header.sequenceNumber, corrected_time_ms,
                                payload_length, false);
    if (last_capture_time_ms_sent_ == 0 ||
        corrected_time_ms > last_capture_time_ms_sent_) {
      last_capture_time_ms_sent_ = corrected_time_ms;
      logv("PacedSend: %d capture_time_ms: %d", corrected_time_ms, corrected_time_ms);
    }
    return 0;
  }

  PacketOptions options;
  if (AllocateTransportSequenceNumber(&options.packet_id)) {
    if (UpdateTransportSequenceNumber(options.packet_id, buffer, length,
                                      rtp_header)) {
      if (transport_feedback_observer_)
        transport_feedback_observer_->AddPacket(options.packet_id, length,
                                                PacketInfo::kNotAProbe);
    }
  }
  UpdateDelayStatistics(capture_time_ms, now_ms);
  UpdateOnSendPacket(options.packet_id, capture_time_ms, rtp_header.ssrc);

  bool sent = SendPacketToNetwork(buffer, length, options);

  // Mark the packet as sent in the history even if send failed. Dropping a
  // packet here should be treated as any other packet drop so we should be
  // ready for a retransmission.
  packet_history_.SetSent(rtp_header.sequenceNumber);

  if (!sent)
    return -1;

  {
    AutoLock cs(send_critsect_.get());
    media_has_been_sent_ = true;
  }
  UpdateRtpStats(buffer, length, rtp_header, false, false);
  return 0;
}

void RTPSender::UpdateDelayStatistics(int64_t capture_time_ms, int64_t now_ms) {
  if (!send_side_delay_observer_ || capture_time_ms <= 0)
    return;

  uint32_t ssrc;
  int avg_delay_ms = 0;
  int max_delay_ms = 0;
  {
    AutoLock cs(send_critsect_.get());
    ssrc = ssrc_;
  }
  {
    AutoLock cs(statistics_crit_.get());
    // TODO(holmer): Compute this iteratively instead.
    send_delays_[now_ms] = now_ms - capture_time_ms;
    send_delays_.erase(send_delays_.begin(),
                       send_delays_.lower_bound(now_ms -
                       kSendSideDelayWindowMs));
    int num_delays = 0;
    for (auto it = send_delays_.upper_bound(now_ms - kSendSideDelayWindowMs);
         it != send_delays_.end(); ++it) {
      max_delay_ms = std::max(max_delay_ms, it->second);
      avg_delay_ms += it->second;
      ++num_delays;
    }
    if (num_delays == 0)
      return;
    avg_delay_ms = (avg_delay_ms + num_delays / 2) / num_delays;
  }
  send_side_delay_observer_->SendSideDelayUpdated(avg_delay_ms, max_delay_ms,
                                                  ssrc);
}

void RTPSender::UpdateOnSendPacket(int packet_id,
                                   int64_t capture_time_ms,
                                   uint32_t ssrc) {
  if (!send_packet_observer_ || capture_time_ms <= 0 || packet_id == -1)
    return;

  send_packet_observer_->OnSendPacket(packet_id, capture_time_ms, ssrc);
}

void RTPSender::ProcessBitrate() {
  if (!bitrate_callback_)
    return;
  int64_t now_ms = clock_->TimeInMilliseconds();
  uint32_t ssrc;
  {
    AutoLock cs(send_critsect_.get());
    ssrc = ssrc_;
  }

  AutoLock cs(statistics_crit_.get());
  bitrate_callback_->Notify(total_bitrate_sent_.Rate(now_ms).value_or(0),
                            nack_bitrate_sent_.Rate(now_ms).value_or(0), ssrc);
}

size_t RTPSender::RtpHeaderLength() const {
  AutoLock cs(send_critsect_.get());
  size_t rtp_header_length = kRtpHeaderLength;
  rtp_header_length += sizeof(uint32_t) * csrcs_.size();
  rtp_header_length += RtpHeaderExtensionLength();
  return rtp_header_length;
}

uint16_t RTPSender::AllocateSequenceNumber(uint16_t packets_to_send) {
  AutoLock cs(send_critsect_.get());
  uint16_t first_allocated_sequence_number = sequence_number_;
  sequence_number_ += packets_to_send;
  return first_allocated_sequence_number;
}

void RTPSender::GetDataCounters(StreamDataCounters* rtp_stats,
                                StreamDataCounters* rtx_stats) const {
  AutoLock cs(statistics_crit_.get());
  *rtp_stats = rtp_stats_;
  *rtx_stats = rtx_rtp_stats_;
}

size_t RTPSender::CreateRtpHeader(uint8_t* header,
                                  int8_t payload_type,
                                  uint32_t ssrc,
                                  bool marker_bit,
                                  uint32_t timestamp,
                                  uint16_t sequence_number,
                                  const std::vector<uint32_t>& csrcs) const {
  header[0] = 0x80;  // version 2.
  header[1] = static_cast<uint8_t>(payload_type);
  if (marker_bit) {
    header[1] |= kRtpMarkerBitMask;  // Marker bit is set.
  }
  ByteWriter<uint16_t>::WriteBigEndian(header + 2, sequence_number);
  ByteWriter<uint32_t>::WriteBigEndian(header + 4, timestamp);
  ByteWriter<uint32_t>::WriteBigEndian(header + 8, ssrc);
  int32_t rtp_header_length = kRtpHeaderLength;

  if (csrcs.size() > 0) {
    uint8_t* ptr = &header[rtp_header_length];
    for (size_t i = 0; i < csrcs.size(); ++i) {
      ByteWriter<uint32_t>::WriteBigEndian(ptr, csrcs[i]);
      ptr += 4;
    }
    header[0] = (header[0] & 0xf0) | csrcs.size();

    // Update length of header.
    rtp_header_length += sizeof(uint32_t) * csrcs.size();
  }

  uint16_t len =
      BuildRTPHeaderExtension(header + rtp_header_length, marker_bit);
  if (len > 0) {
    header[0] |= 0x10;  // Set extension bit.
    rtp_header_length += len;
  }
  return rtp_header_length;
}

int32_t RTPSender::BuildRTPheader(uint8_t* data_buffer,
                                  int8_t payload_type,
                                  bool marker_bit,
                                  uint32_t capture_timestamp,
                                  int64_t capture_time_ms,
                                  bool timestamp_provided,
                                  bool inc_sequence_number) {
  assert(payload_type >= 0);
  AutoLock cs(send_critsect_.get());

  if (timestamp_provided) {
    timestamp_ = start_timestamp_ + capture_timestamp;
  } else {
    // Make a unique time stamp.
    // We can't inc by the actual time, since then we increase the risk of back
    // timing.
    timestamp_++;
  }
  last_timestamp_time_ms_ = clock_->TimeInMilliseconds();
  uint32_t sequence_number = sequence_number_++;
  capture_time_ms_ = capture_time_ms;
  last_packet_marker_bit_ = marker_bit;
  return CreateRtpHeader(data_buffer, payload_type, ssrc_, marker_bit,
                         timestamp_, sequence_number, csrcs_);
}

uint16_t RTPSender::BuildRTPHeaderExtension(uint8_t* data_buffer,
                                            bool marker_bit) const {
  if (rtp_header_extension_map_.Size() <= 0) {
    return 0;
  }
  // RTP header extension, RFC 3550.
  //   0                   1                   2                   3
  //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |      defined by profile       |           length              |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |                        header extension                       |
  //  |                             ....                              |
  //
  const uint32_t kPosLength = 2;
  const uint32_t kHeaderLength = kRtpOneByteHeaderLength;

  // Add extension ID (0xBEDE).
  ByteWriter<uint16_t>::WriteBigEndian(data_buffer,
                                       kRtpOneByteHeaderExtensionId);

  // Add extensions.
  uint16_t total_block_length = 0;

  RTPExtensionType type = rtp_header_extension_map_.First();
  while (type != kRtpExtensionNone) {
    uint8_t block_length = 0;
    uint8_t* extension_data = &data_buffer[kHeaderLength + total_block_length];
    switch (type) {
      case kRtpExtensionTransmissionTimeOffset:
        block_length = BuildTransmissionTimeOffsetExtension(extension_data);
        break;
      case kRtpExtensionAudioLevel:
        block_length = BuildAudioLevelExtension(extension_data);
        break;
      case kRtpExtensionAbsoluteSendTime:
        block_length = BuildAbsoluteSendTimeExtension(extension_data);
        break;
      case kRtpExtensionVideoRotation:
        block_length = BuildVideoRotationExtension(extension_data);
        break;
      case kRtpExtensionTransportSequenceNumber:
        block_length = BuildTransportSequenceNumberExtension(
            extension_data, transport_sequence_number_);
        break;
      case kRtpExtensionPlayoutDelay:
        block_length = BuildPlayoutDelayExtension(
            extension_data, playout_delay_oracle_.min_playout_delay_ms(),
            playout_delay_oracle_.max_playout_delay_ms());
        break;
      default:
        assert(false);
    }
    total_block_length += block_length;
    type = rtp_header_extension_map_.Next(type);
  }
  if (total_block_length == 0) {
    // No extension added.
    return 0;
  }
  // Add padding elements until we've filled a 32 bit block.
  size_t padding_bytes =
      RtpUtility::Word32Align(total_block_length) - total_block_length;
  if (padding_bytes > 0) {
    memset(&data_buffer[kHeaderLength + total_block_length], 0, padding_bytes);
    total_block_length += padding_bytes;
  }
  // Set header length (in number of Word32, header excluded).
  ByteWriter<uint16_t>::WriteBigEndian(data_buffer + kPosLength,
                                       total_block_length / 4);
  // Total added length.
  return kHeaderLength + total_block_length;
}

uint8_t RTPSender::BuildTransmissionTimeOffsetExtension(
    uint8_t* data_buffer) const {
  // From RFC 5450: Transmission Time Offsets in RTP Streams.
  //
  // The transmission time is signaled to the receiver in-band using the
  // general mechanism for RTP header extensions [RFC5285]. The payload
  // of this extension (the transmitted value) is a 24-bit signed integer.
  // When added to the RTP timestamp of the packet, it represents the
  // "effective" RTP transmission time of the packet, on the RTP
  // timescale.
  //
  // The form of the transmission offset extension block:
  //
  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |  ID   | len=2 |              transmission offset              |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  // Get id defined by user.
  uint8_t id;
  if (rtp_header_extension_map_.GetId(kRtpExtensionTransmissionTimeOffset,
                                      &id) != 0) {
    // Not registered.
    return 0;
  }
  size_t pos = 0;
  const uint8_t len = 2;
  data_buffer[pos++] = (id << 4) + len;
  ByteWriter<int32_t, 3>::WriteBigEndian(data_buffer + pos,
                                         transmission_time_offset_);
  pos += 3;
  assert(pos == kTransmissionTimeOffsetLength);
  return kTransmissionTimeOffsetLength;
}

uint8_t RTPSender::BuildAudioLevelExtension(uint8_t* data_buffer) const {
  // An RTP Header Extension for Client-to-Mixer Audio Level Indication
  //
  // https://datatracker.ietf.org/doc/draft-lennox-avt-rtp-audio-level-exthdr/
  //
  // The form of the audio level extension block:
  //
  //    0                   1
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |  ID   | len=0 |V|   level     |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //

  // Get id defined by user.
  uint8_t id;
  if (rtp_header_extension_map_.GetId(kRtpExtensionAudioLevel, &id) != 0) {
    // Not registered.
    return 0;
  }
  size_t pos = 0;
  const uint8_t len = 0;
  data_buffer[pos++] = (id << 4) + len;
  data_buffer[pos++] = (1 << 7) + 0;     // Voice, 0 dBov.
  assert(pos == kAudioLevelLength);
  return kAudioLevelLength;
}

uint8_t RTPSender::BuildAbsoluteSendTimeExtension(uint8_t* data_buffer) const {
  // Absolute send time in RTP streams.
  //
  // The absolute send time is signaled to the receiver in-band using the
  // general mechanism for RTP header extensions [RFC5285]. The payload
  // of this extension (the transmitted value) is a 24-bit unsigned integer
  // containing the sender's current time in seconds as a fixed point number
  // with 18 bits fractional part.
  //
  // The form of the absolute send time extension block:
  //
  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |  ID   | len=2 |              absolute send time               |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  // Get id defined by user.
  uint8_t id;
  if (rtp_header_extension_map_.GetId(kRtpExtensionAbsoluteSendTime,
                                      &id) != 0) {
    // Not registered.
    return 0;
  }
  size_t pos = 0;
  const uint8_t len = 2;
  data_buffer[pos++] = (id << 4) + len;
  ByteWriter<uint32_t, 3>::WriteBigEndian(data_buffer + pos,
                                          absolute_send_time_);
  pos += 3;
  assert(pos == kAbsoluteSendTimeLength);
  return kAbsoluteSendTimeLength;
}

uint8_t RTPSender::BuildVideoRotationExtension(uint8_t* data_buffer) const {
  // Coordination of Video Orientation in RTP streams.
  //
  // Coordination of Video Orientation consists in signaling of the current
  // orientation of the image captured on the sender side to the receiver for
  // appropriate rendering and displaying.
  //
  //    0                   1
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |  ID   | len=0 |0 0 0 0 C F R R|
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //

  // Get id defined by user.
  uint8_t id;
  if (rtp_header_extension_map_.GetId(kRtpExtensionVideoRotation, &id) != 0) {
    // Not registered.
    return 0;
  }
  size_t pos = 0;
  const uint8_t len = 0;
  data_buffer[pos++] = (id << 4) + len;
  data_buffer[pos++] = ConvertVideoRotationToCVOByte(rotation_);
  assert(pos == kVideoRotationLength);
  return kVideoRotationLength;
}

uint8_t RTPSender::BuildTransportSequenceNumberExtension(
    uint8_t* data_buffer,
    uint16_t sequence_number) const {
  //   0                   1                   2
  //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |  ID   | L=1   |transport wide sequence number |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  // Get id defined by user.
  uint8_t id;
  if (rtp_header_extension_map_.GetId(kRtpExtensionTransportSequenceNumber,
                                      &id) != 0) {
    // Not registered.
    return 0;
  }
  size_t pos = 0;
  const uint8_t len = 1;
  data_buffer[pos++] = (id << 4) + len;
  ByteWriter<uint16_t>::WriteBigEndian(data_buffer + pos, sequence_number);
  pos += 2;
  assert(pos == kTransportSequenceNumberLength);
  return kTransportSequenceNumberLength;
}

uint8_t RTPSender::BuildPlayoutDelayExtension(
    uint8_t* data_buffer,
    uint16_t min_playout_delay_ms,
    uint16_t max_playout_delay_ms) const {
  CHECK_LE(min_playout_delay_ms, kPlayoutDelayMaxMs);
  CHECK_LE(max_playout_delay_ms, kPlayoutDelayMaxMs);
  CHECK_LE(min_playout_delay_ms, max_playout_delay_ms);
  //   0                   1                   2                   3
  //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |  ID   | len=2 |   MIN delay           |   MAX delay           |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  uint8_t id;
  if (rtp_header_extension_map_.GetId(kRtpExtensionPlayoutDelay, &id) != 0) {
    // Not registered.
    return 0;
  }
  size_t pos = 0;
  const uint8_t len = 2;
  // Convert MS to value to be sent on extension header.
  uint16_t min_playout = min_playout_delay_ms / kPlayoutDelayGranularityMs;
  uint16_t max_playout = max_playout_delay_ms / kPlayoutDelayGranularityMs;

  data_buffer[pos++] = (id << 4) + len;
  data_buffer[pos++] = min_playout >> 4;
  data_buffer[pos++] = ((min_playout & 0xf) << 4) | (max_playout >> 8);
  data_buffer[pos++] = max_playout & 0xff;
  assert(pos == kPlayoutDelayLength);
  return kPlayoutDelayLength;
}

bool RTPSender::FindHeaderExtensionPosition(RTPExtensionType type,
                                            const uint8_t* rtp_packet,
                                            size_t rtp_packet_length,
                                            const RTPHeader& rtp_header,
                                            size_t* position) const {
  // Get length until start of header extension block.
  int extension_block_pos =
      rtp_header_extension_map_.GetLengthUntilBlockStartInBytes(type);
  if (extension_block_pos < 0) {
    LOG(LS_WARNING) << "Failed to find extension position for " << type
                    << " as it is not registered.";
    return false;
  }

  HeaderExtension header_extension(type);

  size_t extension_pos =
      kRtpHeaderLength + rtp_header.numCSRCs * sizeof(uint32_t);
  size_t block_pos = extension_pos + extension_block_pos;
  if (rtp_packet_length < block_pos + header_extension.length ||
      rtp_header.headerLength < block_pos + header_extension.length) {
    LOG(LS_WARNING) << "Failed to find extension position for " << type
                    << " as the length is invalid.";
    return false;
  }

  // Verify that header contains extension.
  if (!(rtp_packet[extension_pos] == 0xBE &&
        rtp_packet[extension_pos + 1] == 0xDE)) {
    LOG(LS_WARNING) << "Failed to find extension position for " << type
                    << "as hdr extension not found.";
    return false;
  }

  *position = block_pos;
  return true;
}

RTPSender::ExtensionStatus RTPSender::VerifyExtension(
    RTPExtensionType extension_type,
    uint8_t* rtp_packet,
    size_t rtp_packet_length,
    const RTPHeader& rtp_header,
    size_t extension_length_bytes,
    size_t* extension_offset) const {
  // Get id.
  uint8_t id = 0;
  if (rtp_header_extension_map_.GetId(extension_type, &id) != 0)
    return ExtensionStatus::kNotRegistered;

  size_t block_pos = 0;
  if (!FindHeaderExtensionPosition(extension_type, rtp_packet,
                                   rtp_packet_length, rtp_header, &block_pos))
    return ExtensionStatus::kError;

  // Verify first byte in block.
  const uint8_t first_block_byte = (id << 4) + (extension_length_bytes - 2);
  if (rtp_packet[block_pos] != first_block_byte)
    return ExtensionStatus::kError;

  *extension_offset = block_pos;
  return ExtensionStatus::kOk;
}

void RTPSender::UpdateTransmissionTimeOffset(uint8_t* rtp_packet,
                                             size_t rtp_packet_length,
                                             const RTPHeader& rtp_header,
                                             int64_t time_diff_ms) const {
  size_t offset;
  AutoLock cs(send_critsect_.get());
  switch (VerifyExtension(kRtpExtensionTransmissionTimeOffset, rtp_packet,
                          rtp_packet_length, rtp_header,
                          kTransmissionTimeOffsetLength, &offset)) {
    case ExtensionStatus::kNotRegistered:
      return;
    case ExtensionStatus::kError:
      LOG(LS_WARNING) << "Failed to update transmission time offset.";
      return;
    case ExtensionStatus::kOk:
      break;
    default:
      //RTC_NOTREACHED();
      break;
  }

  // Update transmission offset field (converting to a 90 kHz timestamp).
  ByteWriter<int32_t, 3>::WriteBigEndian(rtp_packet + offset + 1,
                                         time_diff_ms * 90);  // RTP timestamp.
}

bool RTPSender::UpdateAudioLevel(uint8_t* rtp_packet,
                                 size_t rtp_packet_length,
                                 const RTPHeader& rtp_header,
                                 bool is_voiced,
                                 uint8_t dBov) const {
  size_t offset;
  AutoLock cs(send_critsect_.get());

  switch (VerifyExtension(kRtpExtensionAudioLevel, rtp_packet,
                          rtp_packet_length, rtp_header, kAudioLevelLength,
                          &offset)) {
    case ExtensionStatus::kNotRegistered:
      return false;
    case ExtensionStatus::kError:
      LOG(LS_WARNING) << "Failed to update audio level.";
      return false;
    case ExtensionStatus::kOk:
      break;
    default:
      //RTC_NOTREACHED();
      break;
  }

  rtp_packet[offset + 1] = (is_voiced ? 0x80 : 0x00) + (dBov & 0x7f);
  return true;
}

bool RTPSender::UpdateVideoRotation(uint8_t* rtp_packet,
                                    size_t rtp_packet_length,
                                    const RTPHeader& rtp_header,
                                    VideoRotation rotation) const {
  size_t offset;
  AutoLock cs(send_critsect_.get());

  switch (VerifyExtension(kRtpExtensionVideoRotation, rtp_packet,
                          rtp_packet_length, rtp_header, kVideoRotationLength,
                          &offset)) {
    case ExtensionStatus::kNotRegistered:
      return false;
    case ExtensionStatus::kError:
      LOG(LS_WARNING) << "Failed to update CVO.";
      return false;
    case ExtensionStatus::kOk:
      break;
    default:
      //RTC_NOTREACHED();
      break;
  }

  rtp_packet[offset + 1] = ConvertVideoRotationToCVOByte(rotation);
  return true;
}

void RTPSender::UpdateAbsoluteSendTime(uint8_t* rtp_packet,
                                       size_t rtp_packet_length,
                                       const RTPHeader& rtp_header,
                                       int64_t now_ms) const {
  size_t offset;
  AutoLock cs(send_critsect_.get());

  switch (VerifyExtension(kRtpExtensionAbsoluteSendTime, rtp_packet,
                          rtp_packet_length, rtp_header,
                          kAbsoluteSendTimeLength, &offset)) {
    case ExtensionStatus::kNotRegistered:
      return;
    case ExtensionStatus::kError:
      LOG(LS_WARNING) << "Failed to update absolute send time";
      return;
    case ExtensionStatus::kOk:
      break;
    default:
      //RTC_NOTREACHED();
      break;
  }

  // Update absolute send time field (convert ms to 24-bit unsigned with 18 bit
  // fractional part).
  ByteWriter<uint32_t, 3>::WriteBigEndian(rtp_packet + offset + 1,
                                          ConvertMsTo24Bits(now_ms));
}

bool RTPSender::UpdateTransportSequenceNumber(
    uint16_t sequence_number,
    uint8_t* rtp_packet,
    size_t rtp_packet_length,
    const RTPHeader& rtp_header) const {
  size_t offset;
  AutoLock cs(send_critsect_.get());

  switch (VerifyExtension(kRtpExtensionTransportSequenceNumber, rtp_packet,
                          rtp_packet_length, rtp_header,
                          kTransportSequenceNumberLength, &offset)) {
    case ExtensionStatus::kNotRegistered:
      return false;
    case ExtensionStatus::kError:
      LOG(LS_WARNING) << "Failed to update transport sequence number";
      return false;
    case ExtensionStatus::kOk:
      break;
    default:
      //RTC_NOTREACHED();
      break;
  }

  BuildTransportSequenceNumberExtension(rtp_packet + offset, sequence_number);
  return true;
}

bool RTPSender::AllocateTransportSequenceNumber(int* packet_id) const {
  if (!transport_sequence_number_allocator_)
    return false;

  *packet_id = transport_sequence_number_allocator_->AllocateSequenceNumber();
  return true;
}

void RTPSender::SetSendingStatus(bool enabled) {
  if (enabled) {
    uint32_t frequency_hz = SendPayloadFrequency();
    uint32_t RTPtime = CurrentRtp(*clock_, frequency_hz);

    // Will be ignored if it's already configured via API.
    SetStartTimestamp(RTPtime, false);
  } else {
    AutoLock cs(send_critsect_.get());
    if (!ssrc_forced_) {
      // Generate a new SSRC.
      ssrc_db_->ReturnSSRC(ssrc_);
      ssrc_ = ssrc_db_->CreateSSRC();
      CHECK(ssrc_ != 0);
    }
    // Don't initialize seq number if SSRC passed externally.
    if (!sequence_number_forced_ && !ssrc_forced_) {
      // Generate a new sequence number.
      sequence_number_ = random_.Rand(1, kMaxInitRtpSeqNumber);
    }
  }
}

void RTPSender::SetSendingMediaStatus(bool enabled) {
  AutoLock cs(send_critsect_.get());
  sending_media_ = enabled;
}

bool RTPSender::SendingMedia() const {
  AutoLock cs(send_critsect_.get());
  return sending_media_;
}

uint32_t RTPSender::Timestamp() const {
  AutoLock cs(send_critsect_.get());
  return timestamp_;
}

void RTPSender::SetStartTimestamp(uint32_t timestamp, bool force) {
  AutoLock cs(send_critsect_.get());
  if (force) {
    start_timestamp_forced_ = true;
    start_timestamp_ = timestamp;
  } else {
    if (!start_timestamp_forced_) {
      start_timestamp_ = timestamp;
    }
  }
}

uint32_t RTPSender::StartTimestamp() const {
  AutoLock cs(send_critsect_.get());
  return start_timestamp_;
}

uint32_t RTPSender::GenerateNewSSRC() {
  // If configured via API, return 0.
  AutoLock cs(send_critsect_.get());

  if (ssrc_forced_) {
    return 0;
  }
  ssrc_ = ssrc_db_->CreateSSRC();
  CHECK(ssrc_ != 0);
  return ssrc_;
}

void RTPSender::SetSSRC(uint32_t ssrc) {
  // This is configured via the API.
  AutoLock cs(send_critsect_.get());

  if (ssrc_ == ssrc && ssrc_forced_) {
    return;  // Since it's same ssrc, don't reset anything.
  }
  ssrc_forced_ = true;
  ssrc_db_->ReturnSSRC(ssrc_);
  ssrc_db_->RegisterSSRC(ssrc);
  ssrc_ = ssrc;
  if (!sequence_number_forced_) {
    sequence_number_ = random_.Rand(1, kMaxInitRtpSeqNumber);
  }
}

uint32_t RTPSender::SSRC() const {
  AutoLock cs(send_critsect_.get());
  return ssrc_;
}

void RTPSender::SetCsrcs(const std::vector<uint32_t>& csrcs) {
  assert(csrcs.size() <= kRtpCsrcSize);
  AutoLock cs(send_critsect_.get());
  csrcs_ = csrcs;
}

void RTPSender::SetSequenceNumber(uint16_t seq) {
  AutoLock cs(send_critsect_.get());
  sequence_number_forced_ = true;
  sequence_number_ = seq;
}

uint16_t RTPSender::SequenceNumber() const {
  AutoLock cs(send_critsect_.get());
  return sequence_number_;
}

// Audio.
int32_t RTPSender::SendTelephoneEvent(uint8_t key,
                                      uint16_t time_ms,
                                      uint8_t level) {
  if (!audio_configured_) {
    return -1;
  }
  return audio_->SendTelephoneEvent(key, time_ms, level);
}

int32_t RTPSender::SetAudioPacketSize(uint16_t packet_size_samples) {
  if (!audio_configured_) {
    return -1;
  }
  return audio_->SetAudioPacketSize(packet_size_samples);
}

int32_t RTPSender::SetAudioLevel(uint8_t level_d_bov) {
  return audio_->SetAudioLevel(level_d_bov);
}

int32_t RTPSender::SetRED(int8_t payload_type) {
  if (!audio_configured_) {
    return -1;
  }
  return audio_->SetRED(payload_type);
}

int32_t RTPSender::RED(int8_t *payload_type) const {
  if (!audio_configured_) {
    return -1;
  }
  return audio_->RED(payload_type);
}

RtpVideoCodecTypes RTPSender::VideoCodecType() const {
  assert(!audio_configured_ && "Sender is an audio stream!");
  return video_->VideoCodecType();
}

void RTPSender::SetGenericFECStatus(bool enable,
                                    uint8_t payload_type_red,
                                    uint8_t payload_type_fec) {
  CHECK(!audio_configured_);
  video_->SetGenericFECStatus(enable, payload_type_red, payload_type_fec);
}

void RTPSender::GenericFECStatus(bool* enable,
                                    uint8_t* payload_type_red,
                                    uint8_t* payload_type_fec) const {
  CHECK(!audio_configured_);
  video_->GenericFECStatus(enable, payload_type_red, payload_type_fec);
}

int32_t RTPSender::SetFecParameters(
    const FecProtectionParams *delta_params,
    const FecProtectionParams *key_params) {
  if (audio_configured_) {
    return -1;
  }
  video_->SetFecParameters(delta_params, key_params);
  return 0;
}

void RTPSender::BuildRtxPacket(uint8_t* buffer, size_t* length,
                               uint8_t* buffer_rtx) {
  AutoLock cs(send_critsect_.get());
  uint8_t* data_buffer_rtx = buffer_rtx;
  // Add RTX header.
  RtpUtility::RtpHeaderParser rtp_parser(
      reinterpret_cast<const uint8_t*>(buffer), *length);

  RTPHeader rtp_header;
  rtp_parser.Parse(&rtp_header);

  // Add original RTP header.
  memcpy(data_buffer_rtx, buffer, rtp_header.headerLength);

  // Replace payload type, if a specific type is set for RTX.
  auto kv = rtx_payload_type_map_.find(rtp_header.payloadType);
  // Use rtx mapping associated with media codec if we can't find one, assuming
  // it's red.
  // TODO(holmer): Remove once old Chrome versions don't rely on this.
  if (kv == rtx_payload_type_map_.end())
    kv = rtx_payload_type_map_.find(payload_type_);
  if (kv != rtx_payload_type_map_.end())
    data_buffer_rtx[1] = kv->second;
  if (rtp_header.markerBit)
    data_buffer_rtx[1] |= kRtpMarkerBitMask;

  // Replace sequence number.
  uint8_t* ptr = data_buffer_rtx + 2;
  ByteWriter<uint16_t>::WriteBigEndian(ptr, sequence_number_rtx_++);

  // Replace SSRC.
  ptr += 6;
  ByteWriter<uint32_t>::WriteBigEndian(ptr, ssrc_rtx_);

  // Add OSN (original sequence number).
  ptr = data_buffer_rtx + rtp_header.headerLength;
  ByteWriter<uint16_t>::WriteBigEndian(ptr, rtp_header.sequenceNumber);
  ptr += 2;

  // Add original payload data.
  memcpy(ptr, buffer + rtp_header.headerLength,
         *length - rtp_header.headerLength);
  *length += 2;
}

void RTPSender::RegisterRtpStatisticsCallback(
    StreamDataCountersCallback* callback) {
  AutoLock cs(statistics_crit_.get());
  rtp_stats_callback_ = callback;
}

StreamDataCountersCallback* RTPSender::GetRtpStatisticsCallback() const {
  AutoLock cs(statistics_crit_.get());
  return rtp_stats_callback_;
}

uint32_t RTPSender::BitrateSent() const {
  AutoLock cs(statistics_crit_.get());
  return total_bitrate_sent_.Rate(clock_->TimeInMilliseconds()).value_or(0);
}

void RTPSender::SetRtpState(const RtpState& rtp_state) {
  AutoLock cs(send_critsect_.get());
  sequence_number_ = rtp_state.sequence_number;
  sequence_number_forced_ = true;
  timestamp_ = rtp_state.timestamp;
  capture_time_ms_ = rtp_state.capture_time_ms;
  last_timestamp_time_ms_ = rtp_state.last_timestamp_time_ms;
  media_has_been_sent_ = rtp_state.media_has_been_sent;
}

RtpState RTPSender::GetRtpState() const {
  AutoLock cs(send_critsect_.get());

  RtpState state;
  state.sequence_number = sequence_number_;
  state.start_timestamp = start_timestamp_;
  state.timestamp = timestamp_;
  state.capture_time_ms = capture_time_ms_;
  state.last_timestamp_time_ms = last_timestamp_time_ms_;
  state.media_has_been_sent = media_has_been_sent_;

  return state;
}

void RTPSender::SetRtxRtpState(const RtpState& rtp_state) {
  AutoLock cs(send_critsect_.get());
  sequence_number_rtx_ = rtp_state.sequence_number;
}

RtpState RTPSender::GetRtxRtpState() const {
  AutoLock cs(send_critsect_.get());

  RtpState state;
  state.sequence_number = sequence_number_rtx_;
  state.start_timestamp = start_timestamp_;

  return state;
}

}  // namespace protocol
