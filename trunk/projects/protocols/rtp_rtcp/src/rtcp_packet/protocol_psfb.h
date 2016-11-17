/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef PROTOCOL_RTCP_PACKET_PSFB_H_
#define PROTOCOL_RTCP_PACKET_PSFB_H_

#include "os_typedefs.h"
#include "protocol_rtcp_packet.h"

namespace protocol {
namespace rtcp {

// PSFB: Payload-specific feedback message.
// RFC 4585, Section 6.3.
class Psfb : public RtcpPacket {
 public:
  static constexpr uint8_t kPacketType = 206;

  Psfb() : sender_ssrc_(0), media_ssrc_(0) {}
  ~Psfb() override {}

  void From(uint32_t ssrc) { sender_ssrc_ = ssrc; }
  void To(uint32_t ssrc) { media_ssrc_ = ssrc; }

  uint32_t sender_ssrc() const { return sender_ssrc_; }
  uint32_t media_ssrc() const { return media_ssrc_; }

 protected:
  static constexpr size_t kCommonFeedbackLength = 8;
  void ParseCommonFeedback(const uint8_t* payload);
  void CreateCommonFeedback(uint8_t* payload) const;

 private:
  uint32_t sender_ssrc_;
  uint32_t media_ssrc_;
};

}  // namespace rtcp
}  // namespace protocol
#endif  // PROTOCOL_RTCP_PACKET_PSFB_H_
