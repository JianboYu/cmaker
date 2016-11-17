/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PROTOCOL_RTP_RECEIVER_IMPL_H_
#define PROTOCOL_RTP_RECEIVER_IMPL_H_

#include <memory>

#include "os_typedefs.h"
#include "os_mutex.h"
#include "core_scoped_ptr.h"
#include "protocol_rtp_receiver.h"
#include "protocol_rtp_rtcp_defines.h"
#include "protocol_rtp_receiver_strategy.h"

using namespace core;

namespace protocol {

class RtpReceiverImpl : public RtpReceiver {
 public:
  // Callbacks passed in here may not be NULL (use Null Object callbacks if you
  // want callbacks to do nothing). This class takes ownership of the media
  // receiver but nothing else.
  RtpReceiverImpl(Clock* clock,
                  RtpFeedback* incoming_messages_callback,
                  RTPPayloadRegistry* rtp_payload_registry,
                  RTPReceiverStrategy* rtp_media_receiver);

  virtual ~RtpReceiverImpl();

  int32_t RegisterReceivePayload(const char payload_name[RTP_PAYLOAD_NAME_SIZE],
                                 const int8_t payload_type,
                                 const uint32_t frequency,
                                 const size_t channels,
                                 const uint32_t rate) override;

  int32_t DeRegisterReceivePayload(const int8_t payload_type) override;

  bool IncomingRtpPacket(const RTPHeader& rtp_header,
                         const uint8_t* payload,
                         size_t payload_length,
                         PayloadUnion payload_specific,
                         bool in_order) override;

  // Returns the last received timestamp.
  bool Timestamp(uint32_t* timestamp) const override;
  bool LastReceivedTimeMs(int64_t* receive_time_ms) const override;

  uint32_t SSRC() const override;

  int32_t CSRCs(uint32_t array_of_csrc[kRtpCsrcSize]) const override;

  int32_t Energy(uint8_t array_of_energy[kRtpCsrcSize]) const override;

  TelephoneEventHandler* GetTelephoneEventHandler() override;

 private:
  bool HaveReceivedFrame() const;

  void CheckSSRCChanged(const RTPHeader& rtp_header);
  void CheckCSRC(const WebRtcRTPHeader& rtp_header);
  int32_t CheckPayloadChanged(const RTPHeader& rtp_header,
                              const int8_t first_payload_byte,
                              bool* is_red,
                              PayloadUnion* payload);

  Clock* clock_;
  RTPPayloadRegistry* rtp_payload_registry_;
  std::unique_ptr<RTPReceiverStrategy> rtp_media_receiver_;

  RtpFeedback* cb_rtp_feedback_;

  scoped_ptr<Mutex> critical_section_rtp_receiver_;
  int64_t last_receive_time_;
  size_t last_received_payload_length_;

  // SSRCs.
  uint32_t ssrc_;
  uint8_t num_csrcs_;
  uint32_t current_remote_csrc_[kRtpCsrcSize];

  uint32_t last_received_timestamp_;
  int64_t last_received_frame_time_ms_;
  uint16_t last_received_sequence_number_;
};
}  // namespace protocol
#endif  // PROTOCOL_RTP_RECEIVER_IMPL_H_
