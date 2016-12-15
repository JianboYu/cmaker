#ifndef SOFT_AAC_DEC_H_
#define SOFT_AAC_DEC_H_

#include <vector>
#include "simple_soft_omx_component.h"

#include "aacdecoder_lib.h"
#include "drc_presmode_wrap.h"

namespace omxil {

struct SoftAACDecoder : public SimpleSoftOMXComponent {
    SoftAACDecoder(const char *name,
            const OMX_CALLBACKTYPE *callbacks,
            OMX_PTR appData,
            OMX_COMPONENTTYPE **component);

protected:
    virtual ~SoftAACDecoder();

    virtual OMX_ERRORTYPE internalGetParameter(
            OMX_INDEXTYPE index, OMX_PTR params);

    virtual OMX_ERRORTYPE internalSetParameter(
            OMX_INDEXTYPE index, const OMX_PTR params);

    virtual void onQueueFilled(OMX_U32 portIndex);
    virtual void onPortFlushCompleted(OMX_U32 portIndex);
    virtual void onPortEnableCompleted(OMX_U32 portIndex, bool enabled);
    virtual void onReset();

private:
    enum {
        kNumInputBuffers        = 4,
        kNumOutputBuffers       = 4,
        kNumDelayBlocksMax      = 8,
    };

    HANDLE_AACDECODER mAACDecoder;
    CStreamInfo *mStreamInfo;
    bool mIsADTS;
    bool mIsFirst;
    size_t mInputBufferCount;
    size_t mOutputBufferCount;
    bool mSignalledError;
    OMX_BUFFERHEADERTYPE *mLastInHeader;
    int64_t mLastHeaderTimeUs;
    int64_t mNextOutBufferTimeUs;
    std::vector<int32_t> mBufferSizes;
    std::vector<int32_t> mDecodedSizes;
    std::vector<int64_t> mBufferTimestamps;

    CDrcPresModeWrapper mDrcWrap;

    enum {
        NONE,
        AWAITING_DISABLED,
        AWAITING_ENABLED
    } mOutputPortSettingsChange;

    void initPorts();
    int32_t initDecoder();
    bool isConfigured() const;
    void configureDownmix() const;
    void drainDecoder();

//      delay compensation
    bool mEndOfInput;
    bool mEndOfOutput;
    int32_t mOutputDelayCompensated;
    int32_t mOutputDelayRingBufferSize;
    short *mOutputDelayRingBuffer;
    int32_t mOutputDelayRingBufferWritePos;
    int32_t mOutputDelayRingBufferReadPos;
    int32_t mOutputDelayRingBufferFilled;
    bool outputDelayRingBufferPutSamples(INT_PCM *samples, int numSamples);
    int32_t outputDelayRingBufferGetSamples(INT_PCM *samples, int numSamples);
    int32_t outputDelayRingBufferSamplesAvailable();
    int32_t outputDelayRingBufferSpaceLeft();
    void updateTimeStamp(int64_t inHeaderTimesUs);

    DISALLOW_EVIL_CONSTRUCTORS(SoftAACDecoder);
};

}  // namespace omxil

#endif  // SOFT_AAC_DEC_H_
