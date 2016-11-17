/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PROTOCOL_VIDEO_CODEC_INFORMATION_H_
#define PROTOCOL_VIDEO_CODEC_INFORMATION_H_

#include "protocol_rtp_rtcp_config.h"
#include "protocol_rtp_utility.h"

namespace protocol {
class VideoCodecInformation {
 public:
  virtual void Reset() = 0;

  virtual RtpVideoCodecTypes Type() = 0;
  virtual ~VideoCodecInformation() {}
};
}  // namespace protocol

#endif  // PROTOCOL_VIDEO_CODEC_INFORMATION_H_
