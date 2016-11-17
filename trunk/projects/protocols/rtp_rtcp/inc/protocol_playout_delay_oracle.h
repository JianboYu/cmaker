/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PROTOCOL_PLAYOUT_DELAY_ORACLE_H_
#define PROTOCOL_PLAYOUT_DELAY_ORACLE_H_

#include "os_typedefs.h"
#include "os_mutex.h"
#include "core_scoped_ptr.h"
#include "protocol_common_types.h"
#include "protocol_rtp_rtcp_defines.h"

using namespace os;
using namespace core;

namespace protocol {

// This class tracks the application requests to limit minimum and maximum
// playout delay and makes a decision on whether the current RTP frame
// should include the playout out delay extension header.
//
//  Playout delay can be defined in terms of capture and render time as follows:
//
// Render time = Capture time in receiver time + playout delay
//
// The application specifies a minimum and maximum limit for the playout delay
// which are both communicated to the receiver and the receiver can adapt
// the playout delay within this range based on observed network jitter.
class PlayoutDelayOracle {
 public:
  PlayoutDelayOracle();
  ~PlayoutDelayOracle();

  // Returns true if the current frame should include the playout delay
  // extension
  bool send_playout_delay() const {
    AutoLock lock(crit_sect_.get());
    return send_playout_delay_;
  }

  // Returns current minimum playout delay in milliseconds.
  int min_playout_delay_ms() const {
    AutoLock lock(crit_sect_.get());
    return min_playout_delay_ms_;
  }

  // Returns current maximum playout delay in milliseconds.
  int max_playout_delay_ms() const {
    AutoLock lock(crit_sect_.get());
    return max_playout_delay_ms_;
  }

  // Updates the application requested playout delay, current ssrc
  // and the current sequence number.
  void UpdateRequest(uint32_t ssrc,
                     PlayoutDelay playout_delay,
                     uint16_t seq_num);

  void OnReceivedRtcpReportBlocks(const ReportBlockList& report_blocks);

 private:
  // The playout delay information is updated from the encoder thread(s).
  // The sequence number feedback is updated from the worker thread.
  // Guards access to data across multiple threads.
  scoped_ptr<Mutex> crit_sect_;
  // The current highest sequence number on which playout delay has been sent.
  int64_t high_sequence_number_ GUARDED_BY(crit_sect_);
  // Indicates whether the playout delay should go on the next frame.
  bool send_playout_delay_ GUARDED_BY(crit_sect_);
  // Sender ssrc.
  uint32_t ssrc_ GUARDED_BY(crit_sect_);
  // Sequence number unwrapper.
  SequenceNumberUnwrapper unwrapper_ GUARDED_BY(crit_sect_);
  // Min playout delay value on the next frame if |send_playout_delay_| is set.
  int min_playout_delay_ms_ GUARDED_BY(crit_sect_);
  // Max playout delay value on the next frame if |send_playout_delay_| is set.
  int max_playout_delay_ms_ GUARDED_BY(crit_sect_);

  DISALLOW_COPY_AND_ASSIGN(PlayoutDelayOracle);
};

}  // namespace protocol

#endif  // PROTOCOL_PLAYOUT_DELAY_ORACLE_H_
