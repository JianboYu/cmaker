/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  Class for storing RTP packets.
 */

#ifndef PROTOCOL_RTP_PACKET_HISTORY_H_
#define PROTOCOL_RTP_PACKET_HISTORY_H_

#include <vector>

#include "os_typedefs.h"
#include "os_mutex.h"
#include "core_scoped_ptr.h"
#include "protocol_rtp_rtcp_defines.h"

using namespace os;
using namespace core;

namespace protocol {

class Clock;

static const size_t kMaxHistoryCapacity = 9600;

class RTPPacketHistory {
 public:
  explicit RTPPacketHistory(Clock* clock);
  ~RTPPacketHistory();

  void SetStorePacketsStatus(bool enable, uint16_t number_to_store);

  bool StorePackets() const;

  // Stores RTP packet.
  int32_t PutRTPPacket(const uint8_t* packet,
                       size_t packet_length,
                       int64_t capture_time_ms,
                       StorageType type);

  // Gets stored RTP packet corresponding to the input sequence number.
  // The packet is copied to the buffer pointed to by ptr_rtp_packet.
  // The rtp_packet_length should show the available buffer size.
  // Returns true if packet is found.
  // packet_length: returns the copied packet length on success.
  // min_elapsed_time_ms: the minimum time that must have elapsed since the last
  // time the packet was resent (parameter is ignored if set to zero).
  // If the packet is found but the minimum time has not elapsed, no bytes are
  // copied.
  // stored_time_ms: returns the time when the packet was stored.
  bool GetPacketAndSetSendTime(uint16_t sequence_number,
                               int64_t min_elapsed_time_ms,
                               bool retransmit,
                               uint8_t* packet,
                               size_t* packet_length,
                               int64_t* stored_time_ms);

  bool GetBestFittingPacket(uint8_t* packet, size_t* packet_length,
                            int64_t* stored_time_ms);

  bool HasRTPPacket(uint16_t sequence_number) const;

  bool SetSent(uint16_t sequence_number);

 private:
  void GetPacket(int index,
                 uint8_t* packet,
                 size_t* packet_length,
                 int64_t* stored_time_ms) const;
  void Allocate(size_t number_to_store);
  void Free();
  void VerifyAndAllocatePacketLength(size_t packet_length, uint32_t start_index);
  bool FindSeqNum(uint16_t sequence_number, int32_t* index) const;
  int FindBestFittingPacket(size_t size) const;

 private:
  Clock* clock_;
  scoped_ptr<Mutex> critsect_;
  bool store_ GUARDED_BY(critsect_);
  uint32_t prev_index_ GUARDED_BY(critsect_);

  struct StoredPacket {
    StoredPacket();
    uint16_t sequence_number = 0;
    int64_t time_ms = 0;
    int64_t send_time = 0;
    StorageType storage_type = kDontRetransmit;
    bool has_been_retransmitted = false;

    uint8_t data[IP_PACKET_SIZE];
    size_t length = 0;
  };
  std::vector<StoredPacket> stored_packets_ GUARDED_BY(critsect_);
};
}  // namespace protocol
#endif  // PROTOCOL_RTP_PACKET_HISTORY_H_
