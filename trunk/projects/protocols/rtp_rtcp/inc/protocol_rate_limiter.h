/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PROTOCOL_RATE_LIMITER_H_
#define PROTOCOL_RATE_LIMITER_H_

#include <limits>

#include "os_mutex.h"
#include "core_scoped_ptr.h"
#include "core_constructor.h"
#include "protocol_rate_statistics.h"
#include "protocol_clock.h"

using namespace os;
using namespace core;

namespace protocol {

// Class used to limit a bitrate, making sure the average does not exceed a
// maximum as measured over a sliding window. This class is thread safe; all
// methods will acquire (the same) lock befeore executing.
class RateLimiter {
 public:
  RateLimiter(Clock* clock, int64_t max_window_ms);
  ~RateLimiter();

  // Try to use rate to send bytes. Returns true on success and if so updates
  // current rate.
  bool TryUseRate(size_t packet_size_bytes);

  // Set the maximum bitrate, in bps, that this limiter allows to send.
  void SetMaxRate(uint32_t max_rate_bps);

  // Set the window size over which to measure the current bitrate.
  // For example, irt retransmissions, this is typically the RTT.
  // Returns true on success and false if window_size_ms is out of range.
  bool SetWindowSize(int64_t window_size_ms);

 private:
  Clock* const clock_;
  scoped_ptr<Mutex> lock_;
  RateStatistics current_rate_ GUARDED_BY(lock_);
  int64_t window_size_ms_ GUARDED_BY(lock_);
  uint32_t max_rate_bps_ GUARDED_BY(lock_);

  DISALLOW_IMPLICIT_CONSTRUCTORS(RateLimiter);
};

}  // namespace protocol

#endif  // PROTOCOL_RATE_LIMITER_H_
