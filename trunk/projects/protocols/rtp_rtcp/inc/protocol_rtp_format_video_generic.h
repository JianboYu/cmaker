/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef PROTOCOL_RTP_FORMAT_VIDEO_GENERIC_H_
#define PROTOCOL_RTP_FORMAT_VIDEO_GENERIC_H_

#include <string>

#include "os_typedefs.h"
#include "core_constructor.h"
#include "protocol_common_types.h"
#include "protocol_rtp_format.h"

namespace protocol {
namespace RtpFormatVideoGeneric {
static const uint8_t kKeyFrameBit = 0x01;
static const uint8_t kFirstPacketBit = 0x02;
}  // namespace RtpFormatVideoGeneric

class RtpPacketizerGeneric : public RtpPacketizer {
 public:
  // Initialize with payload from encoder.
  // The payload_data must be exactly one encoded generic frame.
  RtpPacketizerGeneric(FrameType frametype, size_t max_payload_len);

  virtual ~RtpPacketizerGeneric();

  void SetPayloadData(const uint8_t* payload_data,
                      size_t payload_size,
                      const RTPFragmentationHeader* fragmentation) override;

  // Get the next payload with generic payload header.
  // buffer is a pointer to where the output will be written.
  // bytes_to_send is an output variable that will contain number of bytes
  // written to buffer. The parameter last_packet is true for the last packet of
  // the frame, false otherwise (i.e., call the function again to get the
  // next packet).
  // Returns true on success or false if there was no payload to packetize.
  bool NextPacket(uint8_t* buffer,
                  size_t* bytes_to_send,
                  bool* last_packet) override;

  ProtectionType GetProtectionType() override;

  StorageType GetStorageType(uint32_t retransmission_settings) override;

  std::string ToString() override;

 private:
  const uint8_t* payload_data_;
  size_t payload_size_;
  const size_t max_payload_len_;
  FrameType frame_type_;
  size_t payload_length_;
  uint8_t generic_header_;

  DISALLOW_COPY_AND_ASSIGN(RtpPacketizerGeneric);
};

// Depacketizer for generic codec.
class RtpDepacketizerGeneric : public RtpDepacketizer {
 public:
  virtual ~RtpDepacketizerGeneric() {}

  bool Parse(ParsedPayload* parsed_payload,
             const uint8_t* payload_data,
             size_t payload_data_length) override;
};
}  // namespace protocol
#endif  // PROTOCOL_RTP_FORMAT_VIDEO_GENERIC_H_