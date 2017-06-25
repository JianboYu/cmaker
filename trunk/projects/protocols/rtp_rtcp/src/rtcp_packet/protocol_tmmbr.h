/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PROTOCOL_RTCP_PACKET_TMMBR_H_
#define PROTOCOL_RTCP_PACKET_TMMBR_H_

#include <vector>

#include "os_typedefs.h"
#include "core_constructor.h"
#include "protocol_tmmb_item.h"
#include "rtcp_packet/protocol_rtpfb.h"

namespace protocol {
namespace rtcp {
class CommonHeader;

// Temporary Maximum Media Stream Bit Rate Request (TMMBR).
// RFC 5104, Section 4.2.1.
class Tmmbr : public Rtpfb {
 public:
  static constexpr uint8_t kFeedbackMessageType = 3;

  Tmmbr() {}
  ~Tmmbr() override {}

  // Parse assumes header is already parsed and validated.
  bool Parse(const CommonHeader& packet);

  void WithTmmbr(const TmmbItem& item);

  const std::vector<TmmbItem>& requests() const { return items_; }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  size_t BlockLength() const override {
    return kHeaderLength + kCommonFeedbackLength +
           TmmbItem::kLength * items_.size();
  }

  // Media ssrc is unused, shadow base class setter and getter.
  void To(uint32_t ssrc);
  uint32_t media_ssrc() const;

  std::vector<TmmbItem> items_;

  DISALLOW_COPY_AND_ASSIGN(Tmmbr);
};
}  // namespace rtcp
}  // namespace protocol
#endif  // PROTOCOL_RTCP_PACKET_TMMBR_H_
