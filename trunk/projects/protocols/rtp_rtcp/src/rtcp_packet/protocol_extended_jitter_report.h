/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PROTOCOL_RTCP_PACKET_EXTENDED_JITTER_REPORT_H_
#define PROTOCOL_RTCP_PACKET_EXTENDED_JITTER_REPORT_H_

#include <vector>

#include "core_constructor.h"
#include "protocol_rtcp_packet.h"

namespace protocol {
namespace rtcp {
class CommonHeader;

class ExtendedJitterReport : public RtcpPacket {
 public:
  static constexpr uint8_t kPacketType = 195;

  ExtendedJitterReport() {}
  ~ExtendedJitterReport() override {}

  // Parse assumes header is already parsed and validated.
  bool Parse(const CommonHeader& packet);

  bool WithJitter(uint32_t jitter);

  const std::vector<uint32_t>& jitters() { return inter_arrival_jitters_; }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  static constexpr size_t kMaxNumberOfJitters = 0x1f;
  static constexpr size_t kJitterSizeBytes = 4;

  size_t BlockLength() const override {
    return kHeaderLength + kJitterSizeBytes * inter_arrival_jitters_.size();
  }

  std::vector<uint32_t> inter_arrival_jitters_;

  DISALLOW_COPY_AND_ASSIGN(ExtendedJitterReport);
};

}  // namespace rtcp
}  // namespace protocol
#endif  // PROTOCOL_RTCP_PACKET_EXTENDED_JITTER_REPORT_H_
