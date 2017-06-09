/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <string.h>
#include <memory>

#include <os_log.h>
#ifdef LOGTAG
#undef LOGTAG
#define LOGTAG "RTPReceiverVideo"
#endif

#include <os_assert.h>
#include "protocol_rtp_cvo.h"
#include "protocol_rtp_payload_registry.h"
#include "protocol_rtp_format.h"
#include "protocol_rtp_format_video_generic.h"
#include "protocol_rtp_utility.h"
#include "protocol_rtp_receiver_video.h"

namespace protocol {

RTPReceiverStrategy* RTPReceiverStrategy::CreateVideoStrategy(
    RtpData* data_callback) {
  return new RTPReceiverVideo(data_callback);
}

RTPReceiverVideo::RTPReceiverVideo(RtpData* data_callback)
    : RTPReceiverStrategy(data_callback) {
}

RTPReceiverVideo::~RTPReceiverVideo() {
}

bool RTPReceiverVideo::ShouldReportCsrcChanges(uint8_t payload_type) const {
  // Always do this for video packets.
  return true;
}

int32_t RTPReceiverVideo::OnNewPayloadTypeCreated(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    int8_t payload_type,
    uint32_t frequency) {
  return 0;
}

int32_t RTPReceiverVideo::ParseRtpPacket(WebRtcRTPHeader* rtp_header,
                                         const PayloadUnion& specific_payload,
                                         bool is_red,
                                         const uint8_t* payload,
                                         size_t payload_length,
                                         int64_t timestamp_ms,
                                         bool is_first_packet) {
  logv("rtp Video::ParseRtp seqnum[%u] timestamp[%u]\n",
               rtp_header->header.sequenceNumber,
               rtp_header->header.timestamp);
  rtp_header->type.Video.codec = specific_payload.Video.videoCodecType;

  CHECK_GE(payload_length, rtp_header->header.paddingLength);
  const size_t payload_data_length =
      payload_length - rtp_header->header.paddingLength;

  if (payload == NULL || payload_data_length == 0) {
    return data_callback_->OnReceivedPayloadData(NULL, 0, rtp_header) == 0 ? 0
                                                                           : -1;
  }

  if (first_packet_received_()) {
    logi("Received first video RTP packet\n");
  }

  // We are not allowed to hold a critical section when calling below functions.
  std::unique_ptr<RtpDepacketizer> depacketizer(
      RtpDepacketizer::Create(rtp_header->type.Video.codec));
  if (depacketizer.get() == NULL) {
    loge("Failed to create depacketizer.\n");
    return -1;
  }

  rtp_header->type.Video.isFirstPacket = is_first_packet;
  RtpDepacketizer::ParsedPayload parsed_payload;
  if (!depacketizer->Parse(&parsed_payload, payload, payload_data_length))
    return -1;

  rtp_header->frameType = parsed_payload.frame_type;
  rtp_header->type = parsed_payload.type;
  rtp_header->type.Video.rotation = kVideoRotation_0;

  // Retrieve the video rotation information.
  if (rtp_header->header.extension.hasVideoRotation) {
    rtp_header->type.Video.rotation = ConvertCVOByteToVideoRotation(
        rtp_header->header.extension.videoRotation);
  }

  rtp_header->type.Video.playout_delay =
      rtp_header->header.extension.playout_delay;

  return data_callback_->OnReceivedPayloadData(parsed_payload.payload,
                                               parsed_payload.payload_length,
                                               rtp_header) == 0
             ? 0
             : -1;
}

int RTPReceiverVideo::GetPayloadTypeFrequency() const {
  return kVideoPayloadTypeFrequency;
}

RTPAliveType RTPReceiverVideo::ProcessDeadOrAlive(
    uint16_t last_payload_length) const {
  return kRtpDead;
}

int32_t RTPReceiverVideo::InvokeOnInitializeDecoder(
    RtpFeedback* callback,
    int8_t payload_type,
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const PayloadUnion& specific_payload) const {
  // TODO(pbos): Remove as soon as audio can handle a changing payload type
  // without this callback.
  return 0;
}

}  // namespace protcol
