/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PROTOCOL_RTCP_PACKET_SDES_H_
#define PROTOCOL_RTCP_PACKET_SDES_H_

#include <string>
#include <vector>

#include "os_typedefs.h"
#include "core_constructor.h"
#include "protocol_rtcp_packet.h"

namespace protocol {
namespace rtcp {
class CommonHeader;
// Source Description (SDES) (RFC 3550).
class Sdes : public RtcpPacket {
 public:
  struct Chunk {
    uint32_t ssrc;
    std::string cname;
  };
  static const uint8_t kPacketType = 202;

  Sdes();
  ~Sdes() override;

  // Parse assumes header is already parsed and validated.
  bool Parse(const CommonHeader& packet);

  bool WithCName(uint32_t ssrc, const std::string& cname);

  const std::vector<Chunk>& chunks() const { return chunks_; }

  size_t BlockLength() const override { return block_length_; }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  static const size_t kMaxNumberOfChunks = 0x1f;

  std::vector<Chunk> chunks_;
  size_t block_length_;

  DISALLOW_COPY_AND_ASSIGN(Sdes);
};
}  // namespace rtcp
}  // namespace protocol
#endif  // PROTOCOL_RTCP_PACKET_SDES_H_
