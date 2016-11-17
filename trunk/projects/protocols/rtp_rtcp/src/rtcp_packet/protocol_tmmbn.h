/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PROTOCOL_RTCP_PACKET_TMMBN_H_
#define PROTOCOL_RTCP_PACKET_TMMBN_H_

#include <vector>

#include "os_typedefs.h"
#include "core_constructor.h"
#include "rtcp_packet/protocol_rtpfb.h"
#include "rtcp_packet/protocol_tmmb_item.h"

namespace protocol {
namespace rtcp {
class CommonHeader;

// Temporary Maximum Media Stream Bit Rate Notification (TMMBN).
// RFC 5104, Section 4.2.2.
class Tmmbn : public Rtpfb {
 public:
  static constexpr uint8_t kFeedbackMessageType = 4;

  Tmmbn() {}
  ~Tmmbn() override {}

  // Parse assumes header is already parsed and validated.
  bool Parse(const CommonHeader& packet);

  void WithTmmbr(uint32_t ssrc, uint32_t bitrate_kbps, uint16_t overhead) {
    WithTmmbr(TmmbItem(ssrc, bitrate_kbps * 1000, overhead));
  }
  void WithTmmbr(const TmmbItem& item);

  const std::vector<TmmbItem>& items() const { return items_; }

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

  DISALLOW_COPY_AND_ASSIGN(Tmmbn);
};
}  // namespace rtcp
}  // namespace protocol
#endif  // PROTOCOL_RTCP_PACKET_TMMBN_H_
