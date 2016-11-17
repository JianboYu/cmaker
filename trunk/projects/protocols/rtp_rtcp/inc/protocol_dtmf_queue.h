/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PROTOCOL_DTMF_QUEUE_H_
#define PROTOCOL_DTMF_QUEUE_H_

#include "os_typedefs.h"
#include "os_mutex.h"
#include "core_scoped_ptr.h"
#include "protocol_rtp_rtcp_config.h"

using namespace os;
using namespace core;

namespace protocol {
class DTMFqueue {
 public:
  DTMFqueue();
  virtual ~DTMFqueue();

  int32_t AddDTMF(uint8_t dtmf_key, uint16_t len, uint8_t level);
  int8_t NextDTMF(uint8_t* dtmf_key, uint16_t* len, uint8_t* level);
  bool PendingDTMF();
  void ResetDTMF();

 private:
  scoped_ptr<Mutex> dtmf_critsect_;
  uint8_t next_empty_index_;
  uint8_t dtmf_key_[DTMF_OUTBAND_MAX];
  uint16_t dtmf_length[DTMF_OUTBAND_MAX];
  uint8_t dtmf_level_[DTMF_OUTBAND_MAX];
};
}  // namespace protocol

#endif  // PROTOCOL_DTMF_QUEUE_H_
