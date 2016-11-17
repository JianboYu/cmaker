/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "os_assert.h"
#include "protocol_byte_io.h"
#include "rtcp_packet/protocol_common_header.h"
#include "rtcp_packet/protocol_receiver_report.h"

namespace protocol {
namespace rtcp {
//
// RTCP receiver report (RFC 3550).
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|    RC   |   PT=RR=201   |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                     SSRC of packet sender                     |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  |                         report block(s)                       |
//  |                            ....                               |
bool ReceiverReport::Parse(const CommonHeader& packet) {
  CHECK_EQ(packet.type(), (uint8_t)kPacketType);

  const uint8_t report_blocks_count = packet.count();

  if ((uint32_t)packet.payload_size_bytes() <
      kRrBaseLength + report_blocks_count * ReportBlock::kLength) {
    //LOG(LS_WARNING) << "Packet is too small to contain all the data.";
    return false;
  }

  sender_ssrc_ = ByteReader<uint32_t>::ReadBigEndian(packet.payload());

  const uint8_t* next_report_block = packet.payload() + kRrBaseLength;

  report_blocks_.resize(report_blocks_count);
  for (ReportBlock& block : report_blocks_) {
    block.Parse(next_report_block, ReportBlock::kLength);
    next_report_block += ReportBlock::kLength;
  }

  CHECK_EQ(next_report_block - packet.payload(),
                static_cast<ptrdiff_t>(packet.payload_size_bytes()));
  return true;
}

bool ReceiverReport::Create(uint8_t* packet,
                            size_t* index,
                            size_t max_length,
                            RtcpPacket::PacketReadyCallback* callback) const {
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }
  CreateHeader(report_blocks_.size(), kPacketType, HeaderLength(), packet,
               index);
  ByteWriter<uint32_t>::WriteBigEndian(packet + *index, sender_ssrc_);
  *index += kRrBaseLength;
  for (const ReportBlock& block : report_blocks_) {
    block.Create(packet + *index);
    *index += ReportBlock::kLength;
  }
  return true;
}

bool ReceiverReport::WithReportBlock(const ReportBlock& block) {
  if (report_blocks_.size() >= kMaxNumberOfReportBlocks) {
    //LOG(LS_WARNING) << "Max report blocks reached.";
    return false;
  }
  report_blocks_.push_back(block);
  return true;
}

}  // namespace rtcp
}  // namespace protocol
