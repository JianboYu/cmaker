/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "os_assert.h"
#include "protocol_timeutils.h"
#include "protocol_ssrc_database.h"

namespace protocol {

SSRCDatabase* SSRCDatabase::GetSSRCDatabase() {
  return GetStaticInstance<SSRCDatabase>(kAddRef);
}

void SSRCDatabase::ReturnSSRCDatabase() {
  GetStaticInstance<SSRCDatabase>(kRelease);
}

uint32_t SSRCDatabase::CreateSSRC() {
  AutoLock cs(crit_.get());

  while (true) {  // Try until get a new ssrc.
    // 0 and 0xffffffff are invalid values for SSRC.
    uint32_t ssrc = random_.Rand(1u, 0xfffffffe);
    if (ssrcs_.insert(ssrc).second) {
      return ssrc;
    }
  }
}

void SSRCDatabase::RegisterSSRC(uint32_t ssrc) {
  AutoLock cs(crit_.get());
  ssrcs_.insert(ssrc);
}

void SSRCDatabase::ReturnSSRC(uint32_t ssrc) {
  AutoLock cs(crit_.get());
  ssrcs_.erase(ssrc);
}

SSRCDatabase::SSRCDatabase() : random_(TimeMicros()) {
  crit_.reset(Mutex::Create());
}

SSRCDatabase::~SSRCDatabase() {}

}  // namespace protocol
