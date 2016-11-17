/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PROTOCOL_RTP_UTILITY_H_
#define PROTOCOL_RTP_UTILITY_H_

#include <map>

#include "os_typedefs.h"
#include "protocol_receive_statistics.h"
#include "protocol_rtp_rtcp_defines.h"
#include "protocol_rtp_header_extension.h"
#include "protocol_rtp_rtcp_config.h"

namespace protocol {

const uint8_t kRtpMarkerBitMask = 0x80;

RtpData* NullObjectRtpData();
RtpFeedback* NullObjectRtpFeedback();
ReceiveStatistics* NullObjectReceiveStatistics();

namespace RtpUtility {

struct Payload {
  char name[RTP_PAYLOAD_NAME_SIZE];
  bool audio;
  PayloadUnion typeSpecific;
};

typedef std::map<int8_t, Payload*> PayloadTypeMap;

bool StringCompare(const char* str1, const char* str2, const uint32_t length);

// Round up to the nearest size that is a multiple of 4.
size_t Word32Align(size_t size);

class RtpHeaderParser {
 public:
  RtpHeaderParser(const uint8_t* rtpData, size_t rtpDataLength);
  ~RtpHeaderParser();

  bool RTCP() const;
  bool ParseRtcp(RTPHeader* header) const;
  bool Parse(RTPHeader* parsedPacket,
             RtpHeaderExtensionMap* ptrExtensionMap = nullptr) const;

 private:
  void ParseOneByteExtensionHeader(RTPHeader* parsedPacket,
                                   const RtpHeaderExtensionMap* ptrExtensionMap,
                                   const uint8_t* ptrRTPDataExtensionEnd,
                                   const uint8_t* ptr) const;

  uint8_t ParsePaddingBytes(const uint8_t* ptrRTPDataExtensionEnd,
                            const uint8_t* ptr) const;

  const uint8_t* const _ptrRTPDataBegin;
  const uint8_t* const _ptrRTPDataEnd;
};
}  // namespace RtpUtility
}  // namespace protocol

#endif  // PROTOCOL_RTP_UTILITY_H_
