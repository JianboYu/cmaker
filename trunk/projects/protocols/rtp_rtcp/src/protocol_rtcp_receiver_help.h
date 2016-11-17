/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PROTOCOL_RECEIVER_HELP_H_
#define PROTOCOL_RECEIVER_HELP_H_

#include <list>
#include <memory>
#include <vector>

#include "os_typedefs.h"
#include "core_constructor.h"
#include "protocol_rtp_rtcp_defines.h"  // RTCPReportBlock
#include "protocol_rtcp_utility.h"
#include "protocol_tmmbr_help.h"

namespace protocol {
namespace rtcp {
class TransportFeedback;
}
namespace RTCPHelp
{

class RTCPReportBlockInformation
{
public:
    RTCPReportBlockInformation();
    ~RTCPReportBlockInformation();

    // Statistics
    RTCPReportBlock remoteReceiveBlock;
    uint32_t        remoteMaxJitter;

    // RTT
    int64_t  RTT;
    int64_t  minRTT;
    int64_t  maxRTT;
    int64_t  avgRTT;
    uint32_t numAverageCalcs;
};

class RTCPPacketInformation
{
public:
    RTCPPacketInformation();
    ~RTCPPacketInformation();

    void AddVoIPMetric(const RTCPVoIPMetric*  metric);

    void AddApplicationData(const uint8_t* data,
                            const uint16_t size);

    void AddNACKPacket(const uint16_t packetID);
    void ResetNACKPacketIdArray();

    void AddReportInfo(const RTCPReportBlockInformation& report_block_info);

    uint32_t  rtcpPacketTypeFlags; // RTCPPacketTypeFlags bit field
    uint32_t  remoteSSRC;

    std::list<uint16_t> nackSequenceNumbers;

    uint8_t   applicationSubType;
    uint32_t  applicationName;
    uint8_t*  applicationData;
    uint16_t  applicationLength;

    ReportBlockList report_blocks;
    int64_t rtt;

    uint32_t  interArrivalJitter;

    uint8_t   sliPictureId;
    uint64_t  rpsiPictureId;
    uint32_t  receiverEstimatedMaxBitrate;

    uint32_t ntp_secs;
    uint32_t ntp_frac;
    uint32_t rtp_timestamp;

    uint32_t xr_originator_ssrc;
    bool xr_dlrr_item;
    std::unique_ptr<RTCPVoIPMetric>  VoIPMetric;

    std::unique_ptr<rtcp::TransportFeedback> transport_feedback_;

private:
    DISALLOW_COPY_AND_ASSIGN(RTCPPacketInformation);
};

class RTCPReceiveInformation
{
public:
    RTCPReceiveInformation();
    ~RTCPReceiveInformation();

    void VerifyAndAllocateBoundingSet(const uint32_t minimumSize);
    void VerifyAndAllocateTMMBRSet(const uint32_t minimumSize);

    void InsertTMMBRItem(const uint32_t senderSSRC,
                         const RTCPUtility::RTCPPacketRTPFBTMMBRItem& TMMBRItem,
                         const int64_t currentTimeMS);

    // get
    int32_t GetTMMBRSet(const uint32_t sourceIdx,
                        const uint32_t targetIdx,
                        TMMBRSet* candidateSet,
                        const int64_t currentTimeMS);

    int64_t lastTimeReceived;

    // FIR
    int32_t lastFIRSequenceNumber;
    int64_t lastFIRRequest;

    // TMMBN
    TMMBRSet        TmmbnBoundingSet;

    // TMMBR
    TMMBRSet        TmmbrSet;

    bool            readyForDelete;
private:
    std::vector<int64_t> _tmmbrSetTimeouts;
};

}  // end namespace RTCPHelp
}  // namespace protocol

#endif // PROTOCOL_RECEIVER_HELP_H_
