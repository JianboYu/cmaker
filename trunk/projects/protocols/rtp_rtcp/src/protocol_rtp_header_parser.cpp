/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "os_mutex.h"
#include "core_scoped_ptr.h"
#include "protocol_rtp_header_extension.h"
#include "protocol_rtp_utility.h"
#include "protocol_rtp_header_parser.h"

using namespace os;
using namespace core;

namespace protocol {

class RtpHeaderParserImpl : public RtpHeaderParser {
 public:
  RtpHeaderParserImpl();
  virtual ~RtpHeaderParserImpl() {}

  bool Parse(const uint8_t* packet,
             size_t length,
             RTPHeader* header) const override;

  bool RegisterRtpHeaderExtension(RTPExtensionType type, uint8_t id) override;

  bool DeregisterRtpHeaderExtension(RTPExtensionType type) override;

 private:
  scoped_ptr<Mutex> critical_section_;
  RtpHeaderExtensionMap rtp_header_extension_map_ GUARDED_BY(critical_section_);
};

RtpHeaderParser* RtpHeaderParser::Create() {
  return new RtpHeaderParserImpl;
}

RtpHeaderParserImpl::RtpHeaderParserImpl() {
  critical_section_.reset(Mutex::Create());
}

bool RtpHeaderParser::IsRtcp(const uint8_t* packet, size_t length) {
  RtpUtility::RtpHeaderParser rtp_parser(packet, length);
  return rtp_parser.RTCP();
}

bool RtpHeaderParserImpl::Parse(const uint8_t* packet,
                                size_t length,
                                RTPHeader* header) const {
  RtpUtility::RtpHeaderParser rtp_parser(packet, length);
  memset(header, 0, sizeof(*header));

  RtpHeaderExtensionMap map;
  {
    AutoLock cs(critical_section_.get());
    rtp_header_extension_map_.GetCopy(&map);
  }

  const bool valid_rtpheader = rtp_parser.Parse(header, &map);
  if (!valid_rtpheader) {
    return false;
  }
  return true;
}

bool RtpHeaderParserImpl::RegisterRtpHeaderExtension(RTPExtensionType type,
                                                     uint8_t id) {
  AutoLock cs(critical_section_.get());
  return rtp_header_extension_map_.Register(type, id) == 0;
}

bool RtpHeaderParserImpl::DeregisterRtpHeaderExtension(RTPExtensionType type) {
  AutoLock cs(critical_section_.get());
  return rtp_header_extension_map_.Deregister(type) == 0;
}
}  // namespace protocol
