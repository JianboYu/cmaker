#ifndef SOFT_AAC_ENC_H_
#define SOFT_AAC_ENC_H_

#include "aacenc_lib.h"
#include "simple_soft_omx_component.h"

namespace omxil {

struct SoftAACEncoder : public SimpleSoftOMXComponent {
    SoftAACEncoder(
            const char *name,
            const OMX_CALLBACKTYPE *callbacks,
            OMX_PTR appData,
            OMX_COMPONENTTYPE **component);

protected:
    virtual ~SoftAACEncoder();

    virtual OMX_ERRORTYPE internalGetParameter(
            OMX_INDEXTYPE index, OMX_PTR params);

    virtual OMX_ERRORTYPE internalSetParameter(
            OMX_INDEXTYPE index, const OMX_PTR params);

    virtual void onQueueFilled(OMX_U32 portIndex);

private:
    enum {
        kNumBuffers             = 4,
        kNumSamplesPerFrame     = 1024
    };

    HANDLE_AACENCODER mAACEncoder;

    OMX_U32 mNumChannels;
    OMX_U32 mSampleRate;
    OMX_U32 mBitRate;
    OMX_S32 mSBRMode;
    OMX_S32 mSBRRatio;
    OMX_U32 mAACProfile;

    bool mSentCodecSpecificData;
    size_t mInputSize;
    int16_t *mInputFrame;
    int64_t mInputTimeUs;

    bool mSawInputEOS;

    bool mSignalledError;

    void initPorts();
    int32_t initEncoder();

    int32_t setAudioParams();

    DISALLOW_EVIL_CONSTRUCTORS(SoftAACEncoder);
};

}  // namespace omxil

#endif  // SOFT_AAC_ENC_H_
