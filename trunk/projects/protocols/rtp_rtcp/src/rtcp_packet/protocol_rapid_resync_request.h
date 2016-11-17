/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PROTOCOL_RTCP_PACKET_RAPID_RESYNC_REQUEST_H_
#define PROTOCOL_RTCP_PACKET_RAPID_RESYNC_REQUEST_H_

#include "os_typedefs.h"
#include "core_constructor.h"
#include "rtcp_packet/protocol_rtpfb.h"

namespace protocol {
namespace rtcp {
class CommonHeader;

// draft-perkins-avt-rapid-rtp-sync-03
class RapidResyncRequest : public Rtpfb {
 public:
  static constexpr uint8_t kFeedbackMessageType = 5;

  RapidResyncRequest() {}
  ~RapidResyncRequest() override {}

  // Parse assumes header is already parsed and validated.
  bool Parse(const CommonHeader& header);

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  size_t BlockLength() const override {
    return kHeaderLength + kCommonFeedbackLength;
  }

  DISALLOW_COPY_AND_ASSIGN(RapidResyncRequest);
};
}  // namespace rtcp
}  // namespace protocol
#endif  // PROTOCOL_RTCP_PACKET_RAPID_RESYNC_REQUEST_H_
