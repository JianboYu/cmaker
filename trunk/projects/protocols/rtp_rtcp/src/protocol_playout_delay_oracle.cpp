/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "protocol_playout_delay_oracle.h"

#include "os_assert.h"
#include "protocol_rtp_rtcp_defines.h"
#include "protocol_rtp_header_extension.h"

namespace protocol {

PlayoutDelayOracle::PlayoutDelayOracle()
    : high_sequence_number_(0),
      send_playout_delay_(false),
      ssrc_(0),
      min_playout_delay_ms_(-1),
      max_playout_delay_ms_(-1) {
    crit_sect_.reset(Mutex::Create());
}

PlayoutDelayOracle::~PlayoutDelayOracle() {}

void PlayoutDelayOracle::UpdateRequest(uint32_t ssrc,
                                       PlayoutDelay playout_delay,
                                       uint16_t seq_num) {
  AutoLock lock(crit_sect_.get());
  CHECK_LE(playout_delay.min_ms, kPlayoutDelayMaxMs);
  CHECK_LE(playout_delay.max_ms, kPlayoutDelayMaxMs);
  CHECK_LE(playout_delay.min_ms, playout_delay.max_ms);
  int64_t unwrapped_seq_num = unwrapper_.Unwrap(seq_num);
  if (playout_delay.min_ms >= 0 &&
      playout_delay.min_ms != min_playout_delay_ms_) {
    send_playout_delay_ = true;
    min_playout_delay_ms_ = playout_delay.min_ms;
    high_sequence_number_ = unwrapped_seq_num;
  }

  if (playout_delay.max_ms >= 0 &&
      playout_delay.max_ms != max_playout_delay_ms_) {
    send_playout_delay_ = true;
    max_playout_delay_ms_ = playout_delay.max_ms;
    high_sequence_number_ = unwrapped_seq_num;
  }
  ssrc_ = ssrc;
}

// If an ACK is received on the packet containing the playout delay extension,
// we stop sending the extension on future packets.
void PlayoutDelayOracle::OnReceivedRtcpReportBlocks(
    const ReportBlockList& report_blocks) {
  AutoLock lock(crit_sect_.get());
  for (const RTCPReportBlock& report_block : report_blocks) {
    if ((ssrc_ == report_block.sourceSSRC) && send_playout_delay_ &&
        (report_block.extendedHighSeqNum > high_sequence_number_)) {
      send_playout_delay_ = false;
    }
  }
}

}  // namespace protocol
