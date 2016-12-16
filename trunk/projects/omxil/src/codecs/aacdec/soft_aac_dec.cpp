#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG "SoftAACDecoder"

#include <cstring>
#include <math.h>
#include <list>
#include <os_typedefs.h>
#include <os_log.h>
#include <os_assert.h>

#include <OMX_AudioExt.h>
#include <OMX_IndexExt.h>
#include "soft_aac_dec.h"

#define UNKNOWN_ERROR (-1)
#define ERROR_MALFORMED (-1)

#define FILEREAD_MAX_LAYERS 2

#define DRC_DEFAULT_MOBILE_REF_LEVEL 64  /* 64*-0.25dB = -16 dB below full scale for mobile conf */
#define DRC_DEFAULT_MOBILE_DRC_CUT   127 /* maximum compression of dynamic range for mobile conf */
#define DRC_DEFAULT_MOBILE_DRC_BOOST 127 /* maximum compression of dynamic range for mobile conf */
#define DRC_DEFAULT_MOBILE_DRC_HEAVY 1   /* switch for heavy compression for mobile conf */
#define DRC_DEFAULT_MOBILE_ENC_LEVEL -1 /* encoder target level; -1 => the value is unknown, otherwise dB step value (e.g. 64 for -16 dB) */
#define MAX_CHANNEL_COUNT            8  /* maximum number of audio channels that can be decoded */
// names of properties that can be used to override the default DRC settings
#define PROP_DRC_OVERRIDE_REF_LEVEL  "aac_drc_reference_level"
#define PROP_DRC_OVERRIDE_CUT        "aac_drc_cut"
#define PROP_DRC_OVERRIDE_BOOST      "aac_drc_boost"
#define PROP_DRC_OVERRIDE_HEAVY      "aac_drc_heavy"
#define PROP_DRC_OVERRIDE_ENC_LEVEL "aac_drc_enc_target_level"

namespace omxil {

template<class T>
static void InitOMXParams(T *params) {
    params->nSize = sizeof(T);
    params->nVersion.s.nVersionMajor = 1;
    params->nVersion.s.nVersionMinor = 0;
    params->nVersion.s.nRevision = 0;
    params->nVersion.s.nStep = 0;
}

SoftAACDecoder::SoftAACDecoder(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component)
    : SimpleSoftOMXComponent(name, callbacks, appData, component),
      mAACDecoder(NULL),
      mStreamInfo(NULL),
      mIsADTS(false),
      mInputBufferCount(0),
      mOutputBufferCount(0),
      mSignalledError(false),
      mLastInHeader(NULL),
      mLastHeaderTimeUs(-1),
      mNextOutBufferTimeUs(0),
      mOutputPortSettingsChange(NONE) {
    initPorts();
    CHECK_EQ(initDecoder(), (int32_t)0);
}

SoftAACDecoder::~SoftAACDecoder() {
    aacDecoder_Close(mAACDecoder);
    delete[] mOutputDelayRingBuffer;
}

void SoftAACDecoder::initPorts() {
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOMXParams(&def);

    def.nPortIndex = 0;
    def.eDir = OMX_DirInput;
    def.nBufferCountMin = kNumInputBuffers;
    def.nBufferCountActual = def.nBufferCountMin;
    def.nBufferSize = 8192;
    def.bEnabled = OMX_TRUE;
    def.bPopulated = OMX_FALSE;
    def.eDomain = OMX_PortDomainAudio;
    def.bBuffersContiguous = OMX_FALSE;
    def.nBufferAlignment = 1;

    //def.format.audio.cMIMEType = const_cast<char *>("audio/aac");
    def.format.audio.pNativeRender = NULL;
    def.format.audio.bFlagErrorConcealment = OMX_FALSE;
    def.format.audio.eEncoding = OMX_AUDIO_CodingAAC;

    addPort(def);

    def.nPortIndex = 1;
    def.eDir = OMX_DirOutput;
    def.nBufferCountMin = kNumOutputBuffers;
    def.nBufferCountActual = def.nBufferCountMin;
    def.nBufferSize = 4096 * MAX_CHANNEL_COUNT;
    def.bEnabled = OMX_TRUE;
    def.bPopulated = OMX_FALSE;
    def.eDomain = OMX_PortDomainAudio;
    def.bBuffersContiguous = OMX_FALSE;
    def.nBufferAlignment = 2;

    //def.format.audio.cMIMEType = const_cast<char *>("audio/raw");
    def.format.audio.pNativeRender = NULL;
    def.format.audio.bFlagErrorConcealment = OMX_FALSE;
    def.format.audio.eEncoding = OMX_AUDIO_CodingPCM;

    addPort(def);
}

int32_t SoftAACDecoder::initDecoder() {
    logv("initDecoder()\n");
    int32_t status = UNKNOWN_ERROR;
    mAACDecoder = aacDecoder_Open(TT_MP4_ADIF, /* num layers */ 1);
    if (mAACDecoder != NULL) {
        mStreamInfo = aacDecoder_GetStreamInfo(mAACDecoder);
        if (mStreamInfo != NULL) {
            status = 0;
        }
    }

    mEndOfInput = false;
    mEndOfOutput = false;
    mOutputDelayCompensated = 0;
    mOutputDelayRingBufferSize = 2048 * MAX_CHANNEL_COUNT * kNumDelayBlocksMax;
    mOutputDelayRingBuffer = new short[mOutputDelayRingBufferSize];
    mOutputDelayRingBufferWritePos = 0;
    mOutputDelayRingBufferReadPos = 0;
    mOutputDelayRingBufferFilled = 0;

    if (mAACDecoder == NULL) {
        loge("AAC decoder is null. TODO: Can not call aacDecoder_SetParam in the following code\n");
    }

    //aacDecoder_SetParam(mAACDecoder, AAC_PCM_LIMITER_ENABLE, 0);

    //init DRC wrapper
    mDrcWrap.setDecoderHandle(mAACDecoder);
    mDrcWrap.submitStreamData(mStreamInfo);

    // for streams that contain metadata, use the mobile profile DRC settings unless overridden by platform properties
    // TODO: change the DRC settings depending on audio output device type (HDMI, loadspeaker, headphone)
    //  DRC_PRES_MODE_WRAP_DESIRED_TARGET
    if (0) {
        unsigned refLevel = 0;
        logv("AAC decoder using desired DRC target reference level of %d instead of %d\n", refLevel,
                DRC_DEFAULT_MOBILE_REF_LEVEL);
        mDrcWrap.setParam(DRC_PRES_MODE_WRAP_DESIRED_TARGET, refLevel);
    } else {
        mDrcWrap.setParam(DRC_PRES_MODE_WRAP_DESIRED_TARGET, DRC_DEFAULT_MOBILE_REF_LEVEL);
    }
    //  DRC_PRES_MODE_WRAP_DESIRED_ATT_FACTOR
    if (0) {
        unsigned cut = 0;
        logv("AAC decoder using desired DRC attenuation factor of %d instead of %d\n", cut,
                DRC_DEFAULT_MOBILE_DRC_CUT);
        mDrcWrap.setParam(DRC_PRES_MODE_WRAP_DESIRED_ATT_FACTOR, cut);
    } else {
        mDrcWrap.setParam(DRC_PRES_MODE_WRAP_DESIRED_ATT_FACTOR, DRC_DEFAULT_MOBILE_DRC_CUT);
    }
    //  DRC_PRES_MODE_WRAP_DESIRED_BOOST_FACTOR
    if (0) {
        unsigned boost = 0;
        logv("AAC decoder using desired DRC boost factor of %d instead of %d\n", boost,
                DRC_DEFAULT_MOBILE_DRC_BOOST);
        mDrcWrap.setParam(DRC_PRES_MODE_WRAP_DESIRED_BOOST_FACTOR, boost);
    } else {
        mDrcWrap.setParam(DRC_PRES_MODE_WRAP_DESIRED_BOOST_FACTOR, DRC_DEFAULT_MOBILE_DRC_BOOST);
    }
    //  DRC_PRES_MODE_WRAP_DESIRED_HEAVY
    if (0) {
        unsigned heavy = 0;
        logv("AAC decoder using desried DRC heavy compression switch of %d instead of %d\n", heavy,
                DRC_DEFAULT_MOBILE_DRC_HEAVY);
        mDrcWrap.setParam(DRC_PRES_MODE_WRAP_DESIRED_HEAVY, heavy);
    } else {
        mDrcWrap.setParam(DRC_PRES_MODE_WRAP_DESIRED_HEAVY, DRC_DEFAULT_MOBILE_DRC_HEAVY);
    }
    // DRC_PRES_MODE_WRAP_ENCODER_TARGET
    if (0) {
        unsigned encoderRefLevel = 0;
        logv("AAC decoder using encoder-side DRC reference level of %d instead of %d\n",
                encoderRefLevel, DRC_DEFAULT_MOBILE_ENC_LEVEL);
        mDrcWrap.setParam(DRC_PRES_MODE_WRAP_ENCODER_TARGET, encoderRefLevel);
    } else {
        mDrcWrap.setParam(DRC_PRES_MODE_WRAP_ENCODER_TARGET, DRC_DEFAULT_MOBILE_ENC_LEVEL);
    }

    return status;
}

OMX_ERRORTYPE SoftAACDecoder::internalGetParameter(
        OMX_INDEXTYPE index, OMX_PTR params) {
    switch (index) {
        case OMX_IndexParamAudioAac:
        {
            OMX_AUDIO_PARAM_AACPROFILETYPE *aacParams =
                (OMX_AUDIO_PARAM_AACPROFILETYPE *)params;

            if (aacParams->nPortIndex != 0) {
                return OMX_ErrorUndefined;
            }

            aacParams->nBitRate = 0;
            aacParams->nAudioBandWidth = 0;
            aacParams->nAACtools = 0;
            aacParams->nAACERtools = 0;
            aacParams->eAACProfile = OMX_AUDIO_AACObjectMain;

            aacParams->eAACStreamFormat =
                mIsADTS
                    ? OMX_AUDIO_AACStreamFormatMP4ADTS
                    : OMX_AUDIO_AACStreamFormatMP4FF;

            aacParams->eChannelMode = OMX_AUDIO_ChannelModeStereo;

            if (!isConfigured()) {
                aacParams->nChannels = 1;
                aacParams->nSampleRate = 44100;
                aacParams->nFrameLength = 0;
            } else {
                aacParams->nChannels = mStreamInfo->numChannels;
                aacParams->nSampleRate = mStreamInfo->sampleRate;
                aacParams->nFrameLength = mStreamInfo->frameSize;
            }

            return OMX_ErrorNone;
        }

        case OMX_IndexParamAudioPcm:
        {
            OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams =
                (OMX_AUDIO_PARAM_PCMMODETYPE *)params;

            if (pcmParams->nPortIndex != 1) {
                return OMX_ErrorUndefined;
            }

            pcmParams->eNumData = OMX_NumericalDataSigned;
            pcmParams->eEndian = OMX_EndianBig;
            pcmParams->bInterleaved = OMX_TRUE;
            pcmParams->nBitPerSample = 16;
            pcmParams->ePCMMode = OMX_AUDIO_PCMModeLinear;
            pcmParams->eChannelMapping[0] = OMX_AUDIO_ChannelLF;
            pcmParams->eChannelMapping[1] = OMX_AUDIO_ChannelRF;
            pcmParams->eChannelMapping[2] = OMX_AUDIO_ChannelCF;
            pcmParams->eChannelMapping[3] = OMX_AUDIO_ChannelLFE;
            pcmParams->eChannelMapping[4] = OMX_AUDIO_ChannelLS;
            pcmParams->eChannelMapping[5] = OMX_AUDIO_ChannelRS;

            if (!isConfigured()) {
                pcmParams->nChannels = 1;
                pcmParams->nSamplingRate = 44100;
            } else {
                pcmParams->nChannels = mStreamInfo->numChannels;
                pcmParams->nSamplingRate = mStreamInfo->sampleRate;
            }

            return OMX_ErrorNone;
        }

        default:
            return SimpleSoftOMXComponent::internalGetParameter(index, params);
    }
}

OMX_ERRORTYPE SoftAACDecoder::internalSetParameter(
        OMX_INDEXTYPE index, const OMX_PTR params) {
    switch ((int)index) {
        case OMX_IndexParamStandardComponentRole:
        {
            const OMX_PARAM_COMPONENTROLETYPE *roleParams =
                (const OMX_PARAM_COMPONENTROLETYPE *)params;

            if (strncmp((const char *)roleParams->cRole,
                        "audio_decoder.aac",
                        OMX_MAX_STRINGNAME_SIZE - 1)) {
                return OMX_ErrorUndefined;
            }

            return OMX_ErrorNone;
        }

        case OMX_IndexParamAudioAac:
        {
            const OMX_AUDIO_PARAM_AACPROFILETYPE *aacParams =
                (const OMX_AUDIO_PARAM_AACPROFILETYPE *)params;

            if (aacParams->nPortIndex != 0) {
                return OMX_ErrorUndefined;
            }

            if (aacParams->eAACStreamFormat == OMX_AUDIO_AACStreamFormatMP4FF) {
                mIsADTS = false;
            } else if (aacParams->eAACStreamFormat
                        == OMX_AUDIO_AACStreamFormatMP4ADTS) {
                mIsADTS = true;
            } else {
                return OMX_ErrorUndefined;
            }

            return OMX_ErrorNone;
        }

        case OMX_IndexParamAudioAndroidAacPresentation:
        {
            const OMX_AUDIO_PARAM_ANDROID_AACPRESENTATIONTYPE *aacPresParams =
                    (const OMX_AUDIO_PARAM_ANDROID_AACPRESENTATIONTYPE *)params;
            // for the following parameters of the OMX_AUDIO_PARAM_AACPROFILETYPE structure,
            // a value of -1 implies the parameter is not set by the application:
            //   nMaxOutputChannels     uses default platform properties, see configureDownmix()
            //   nDrcCut                uses default platform properties, see initDecoder()
            //   nDrcBoost                idem
            //   nHeavyCompression        idem
            //   nTargetReferenceLevel    idem
            //   nEncodedTargetLevel      idem
            if (aacPresParams->nMaxOutputChannels >= 0) {
                int max;
                if (aacPresParams->nMaxOutputChannels >= 8) { max = 8; }
                else if (aacPresParams->nMaxOutputChannels >= 6) { max = 6; }
                else if (aacPresParams->nMaxOutputChannels >= 2) { max = 2; }
                else {
                    // -1 or 0: disable downmix,  1: mono
                    max = aacPresParams->nMaxOutputChannels;
                }
                logv("set nMaxOutputChannels=%d\n", max);
                aacDecoder_SetParam(mAACDecoder, AAC_PCM_MAX_OUTPUT_CHANNELS, max);
            }
            bool updateDrcWrapper = false;
            if (aacPresParams->nDrcBoost >= 0) {
                logv("set nDrcBoost=%d\n", aacPresParams->nDrcBoost);
                mDrcWrap.setParam(DRC_PRES_MODE_WRAP_DESIRED_BOOST_FACTOR,
                        aacPresParams->nDrcBoost);
                updateDrcWrapper = true;
            }
            if (aacPresParams->nDrcCut >= 0) {
                logv("set nDrcCut=%d\n", aacPresParams->nDrcCut);
                mDrcWrap.setParam(DRC_PRES_MODE_WRAP_DESIRED_ATT_FACTOR, aacPresParams->nDrcCut);
                updateDrcWrapper = true;
            }
            if (aacPresParams->nHeavyCompression >= 0) {
                logv("set nHeavyCompression=%d\n", aacPresParams->nHeavyCompression);
                mDrcWrap.setParam(DRC_PRES_MODE_WRAP_DESIRED_HEAVY,
                        aacPresParams->nHeavyCompression);
                updateDrcWrapper = true;
            }
            if (aacPresParams->nTargetReferenceLevel >= 0) {
                logv("set nTargetReferenceLevel=%d\n", aacPresParams->nTargetReferenceLevel);
                mDrcWrap.setParam(DRC_PRES_MODE_WRAP_DESIRED_TARGET,
                        aacPresParams->nTargetReferenceLevel);
                updateDrcWrapper = true;
            }
            if (aacPresParams->nEncodedTargetLevel >= 0) {
                logv("set nEncodedTargetLevel=%d\n", aacPresParams->nEncodedTargetLevel);
                mDrcWrap.setParam(DRC_PRES_MODE_WRAP_ENCODER_TARGET,
                        aacPresParams->nEncodedTargetLevel);
                updateDrcWrapper = true;
            }
            if (aacPresParams->nPCMLimiterEnable >= 0) {
                aacDecoder_SetParam(mAACDecoder, AAC_PCM_LIMITER_ENABLE,
                        (aacPresParams->nPCMLimiterEnable != 0));
            }
            if (updateDrcWrapper) {
                mDrcWrap.update();
            }

            return OMX_ErrorNone;
        }

        case OMX_IndexParamAudioPcm:
        {
            const OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams =
                (OMX_AUDIO_PARAM_PCMMODETYPE *)params;

            if (pcmParams->nPortIndex != 1) {
                return OMX_ErrorUndefined;
            }

            return OMX_ErrorNone;
        }

        default:
            return SimpleSoftOMXComponent::internalSetParameter(index, params);
    }
}

bool SoftAACDecoder::isConfigured() const {
    return mInputBufferCount > 0;
}

void SoftAACDecoder::configureDownmix() const {
    if (0) {
        logi("limiting to stereo output\n");
        aacDecoder_SetParam(mAACDecoder, AAC_PCM_MAX_OUTPUT_CHANNELS, 2);
        // By default, the decoder creates a 5.1 channel downmix signal
        // for seven and eight channel input streams. To enable 6.1 and 7.1 channel output
        // use aacDecoder_SetParam(mAACDecoder, AAC_PCM_MAX_OUTPUT_CHANNELS, -1)
    }
}

bool SoftAACDecoder::outputDelayRingBufferPutSamples(INT_PCM *samples, int32_t numSamples) {
    if (numSamples == 0) {
        return true;
    }
    if (outputDelayRingBufferSpaceLeft() < numSamples) {
        loge("RING BUFFER WOULD OVERFLOW\n");
        return false;
    }
    if (mOutputDelayRingBufferWritePos + numSamples <= mOutputDelayRingBufferSize
            && (mOutputDelayRingBufferReadPos <= mOutputDelayRingBufferWritePos
                    || mOutputDelayRingBufferReadPos > mOutputDelayRingBufferWritePos + numSamples)) {
        // faster memcopy loop without checks, if the preconditions allow this
        for (int32_t i = 0; i < numSamples; i++) {
            mOutputDelayRingBuffer[mOutputDelayRingBufferWritePos++] = samples[i];
        }

        if (mOutputDelayRingBufferWritePos >= mOutputDelayRingBufferSize) {
            mOutputDelayRingBufferWritePos -= mOutputDelayRingBufferSize;
        }
    } else {
        logv("slow SoftAACDecoder::outputDelayRingBufferPutSamples()\n");

        for (int32_t i = 0; i < numSamples; i++) {
            mOutputDelayRingBuffer[mOutputDelayRingBufferWritePos] = samples[i];
            mOutputDelayRingBufferWritePos++;
            if (mOutputDelayRingBufferWritePos >= mOutputDelayRingBufferSize) {
                mOutputDelayRingBufferWritePos -= mOutputDelayRingBufferSize;
            }
        }
    }
    mOutputDelayRingBufferFilled += numSamples;
    return true;
}

int32_t SoftAACDecoder::outputDelayRingBufferGetSamples(INT_PCM *samples, int32_t numSamples) {

    if (numSamples > mOutputDelayRingBufferFilled) {
        loge("RING BUFFER WOULD UNDERRUN\n");
        return -1;
    }

    if (mOutputDelayRingBufferReadPos + numSamples <= mOutputDelayRingBufferSize
            && (mOutputDelayRingBufferWritePos < mOutputDelayRingBufferReadPos
                    || mOutputDelayRingBufferWritePos >= mOutputDelayRingBufferReadPos + numSamples)) {
        // faster memcopy loop without checks, if the preconditions allow this
        if (samples != 0) {
            for (int32_t i = 0; i < numSamples; i++) {
                samples[i] = mOutputDelayRingBuffer[mOutputDelayRingBufferReadPos++];
            }
        } else {
            mOutputDelayRingBufferReadPos += numSamples;
        }
        if (mOutputDelayRingBufferReadPos >= mOutputDelayRingBufferSize) {
            mOutputDelayRingBufferReadPos -= mOutputDelayRingBufferSize;
        }
    } else {
        logv("slow SoftAACDecoder::outputDelayRingBufferGetSamples()\n");

        for (int32_t i = 0; i < numSamples; i++) {
            if (samples != 0) {
                samples[i] = mOutputDelayRingBuffer[mOutputDelayRingBufferReadPos];
            }
            mOutputDelayRingBufferReadPos++;
            if (mOutputDelayRingBufferReadPos >= mOutputDelayRingBufferSize) {
                mOutputDelayRingBufferReadPos -= mOutputDelayRingBufferSize;
            }
        }
    }
    mOutputDelayRingBufferFilled -= numSamples;
    return numSamples;
}

int32_t SoftAACDecoder::outputDelayRingBufferSamplesAvailable() {
    return mOutputDelayRingBufferFilled;
}

int32_t SoftAACDecoder::outputDelayRingBufferSpaceLeft() {
    return mOutputDelayRingBufferSize - outputDelayRingBufferSamplesAvailable();
}

void SoftAACDecoder::updateTimeStamp(int64_t inHeaderTimeUs) {
    // use new input buffer timestamp as Anchor Time if its
    //    a) first buffer or
    //    b) first buffer post seek or
    //    c) different from last buffer timestamp
    //If input buffer timestamp is same as last input buffer timestamp then
    //treat this as a erroneous timestamp and ignore new input buffer
    //timestamp and use last output buffer timestamp as Anchor Time.
    int64_t anchorTimeUs = 0;
    if ((mLastHeaderTimeUs != inHeaderTimeUs)) {
        anchorTimeUs = inHeaderTimeUs;
        mLastHeaderTimeUs = inHeaderTimeUs;
        //Store current buffer's timestamp so that it can used as reference
        //in cases where first frame/buffer is skipped/dropped.
        //e.g to compensate decoder delay
        mNextOutBufferTimeUs = inHeaderTimeUs;
    } else {
        anchorTimeUs = mNextOutBufferTimeUs;
    }
    mBufferTimestamps.push_back(anchorTimeUs);
}

void SoftAACDecoder::onQueueFilled(OMX_U32 /* portIndex */) {
    if (mSignalledError || mOutputPortSettingsChange != NONE) {
        logw("Queued buffer skipped with signal error or port setting change\n");
        return;
    }

    UCHAR* inBuffer[FILEREAD_MAX_LAYERS];
    UINT inBufferLength[FILEREAD_MAX_LAYERS] = {0};
    UINT bytesValid[FILEREAD_MAX_LAYERS] = {0};

    std::list<BufferInfo *> &inQueue = getPortQueue(0);
    std::list<BufferInfo *> &outQueue = getPortQueue(1);

    while ((!inQueue.empty() || mEndOfInput) && !outQueue.empty()) {
        if (!inQueue.empty()) {
            INT_PCM tmpOutBuffer[2048 * MAX_CHANNEL_COUNT];
            BufferInfo *inInfo = *inQueue.begin();
            OMX_BUFFERHEADERTYPE *inHeader = inInfo->mHeader;

            mEndOfInput = (inHeader->nFlags & OMX_BUFFERFLAG_EOS) != 0;

            if (mInputBufferCount == 0 && !(inHeader->nFlags & OMX_BUFFERFLAG_CODECCONFIG)) {
                loge("first buffer should have OMX_BUFFERFLAG_CODECCONFIG set\n");
                inHeader->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
            }
            if ((inHeader->nFlags & OMX_BUFFERFLAG_CODECCONFIG) != 0) {
                BufferInfo *inInfo = *inQueue.begin();
                OMX_BUFFERHEADERTYPE *inHeader = inInfo->mHeader;

                inBuffer[0] = inHeader->pBuffer + inHeader->nOffset;
                inBufferLength[0] = inHeader->nFilledLen;

                AAC_DECODER_ERROR decoderErr =
                    aacDecoder_ConfigRaw(mAACDecoder,
                                         inBuffer,
                                         inBufferLength);

                if (decoderErr != AAC_DEC_OK) {
                    logw("aacDecoder_ConfigRaw decoderErr = 0x%4.4x\n", decoderErr);
                    mSignalledError = true;
                    notify(OMX_EventError, OMX_ErrorUndefined, decoderErr, NULL);
                    return;
                }

                mInputBufferCount++;
                mOutputBufferCount++; // fake increase of outputBufferCount to keep the counters aligned

                inInfo->mOwnedByUs = false;
                inQueue.erase(inQueue.begin());
                mLastInHeader = NULL;
                inInfo = NULL;
                notifyEmptyBufferDone(inHeader);
                inHeader = NULL;

                configureDownmix();
                // Only send out port settings changed event if both sample rate
                // and numChannels are valid.
                if (mStreamInfo->sampleRate && mStreamInfo->numChannels) {
                    logi("Initially configuring decoder: %d Hz, %d channels\n",
                        mStreamInfo->sampleRate,
                        mStreamInfo->numChannels);

                    notify(OMX_EventPortSettingsChanged, 1, 0, NULL);
                    mOutputPortSettingsChange = AWAITING_DISABLED;
                }
                return;
            }

            if (inHeader->nFilledLen == 0) {
                inInfo->mOwnedByUs = false;
                inQueue.erase(inQueue.begin());
                mLastInHeader = NULL;
                inInfo = NULL;
                notifyEmptyBufferDone(inHeader);
                inHeader = NULL;
                continue;
            }

            if (mIsADTS) {
                size_t adtsHeaderSize = 0;
                // skip 30 bits, aac_frame_length follows.
                // ssssssss ssssiiip ppffffPc ccohCCll llllllll lll?????

                const uint8_t *adtsHeader = inHeader->pBuffer + inHeader->nOffset;

                bool signalError = false;
                if (inHeader->nFilledLen < 7) {
                    loge("Audio data too short to contain even the ADTS header. \n"
                            "Got %d bytes.", inHeader->nFilledLen);
                    //hexdump(adtsHeader, inHeader->nFilledLen);
                    signalError = true;
                } else {
                    bool protectionAbsent = (adtsHeader[1] & 1);

                    unsigned aac_frame_length =
                        ((adtsHeader[3] & 3) << 11)
                        | (adtsHeader[4] << 3)
                        | (adtsHeader[5] >> 5);

                    if (inHeader->nFilledLen < aac_frame_length) {
                        loge("Not enough audio data for the complete frame. \n"
                                "Got %d bytes, frame size according to the ADTS "
                                "header is %u bytes.",
                                inHeader->nFilledLen, aac_frame_length);
                        //hexdump(adtsHeader, inHeader->nFilledLen);
                        signalError = true;
                    } else {
                        adtsHeaderSize = (protectionAbsent ? 7 : 9);

                        inBuffer[0] = (UCHAR *)adtsHeader + adtsHeaderSize;
                        inBufferLength[0] = aac_frame_length - adtsHeaderSize;

                        inHeader->nOffset += adtsHeaderSize;
                        inHeader->nFilledLen -= adtsHeaderSize;
                    }
                }

                if (signalError) {
                    mSignalledError = true;
                    notify(OMX_EventError, OMX_ErrorStreamCorrupt, ERROR_MALFORMED, NULL);
                    return;
                }

                // insert buffer size and time stamp
                mBufferSizes.push_back(inBufferLength[0]);
                if (mLastInHeader != inHeader) {
                    updateTimeStamp(inHeader->nTimeStamp);
                    mLastInHeader = inHeader;
                } else {
                    int64_t currentTime = mBufferTimestamps.at(mBufferTimestamps.size() - 1);
                    currentTime += mStreamInfo->aacSamplesPerFrame *
                            1000000ll / mStreamInfo->aacSampleRate;
                    mBufferTimestamps.push_back(currentTime);
                }
            } else {
                inBuffer[0] = inHeader->pBuffer + inHeader->nOffset;
                inBufferLength[0] = inHeader->nFilledLen;
                mLastInHeader = inHeader;
                updateTimeStamp(inHeader->nTimeStamp);
                mBufferSizes.push_back(inHeader->nFilledLen);
            }

            // Fill and decode
            bytesValid[0] = inBufferLength[0];

            INT prevSampleRate = mStreamInfo->sampleRate;
            INT prevNumChannels = mStreamInfo->numChannels;

            aacDecoder_Fill(mAACDecoder,
                            inBuffer,
                            inBufferLength,
                            bytesValid);

            // run DRC check
            mDrcWrap.submitStreamData(mStreamInfo);
            mDrcWrap.update();

            UINT inBufferUsedLength = inBufferLength[0] - bytesValid[0];
            inHeader->nFilledLen -= inBufferUsedLength;
            inHeader->nOffset += inBufferUsedLength;

            AAC_DECODER_ERROR decoderErr;
            int numLoops = 0;
            do {
                if (outputDelayRingBufferSpaceLeft() <
                        (mStreamInfo->frameSize * mStreamInfo->numChannels)) {
                    logv("skipping decode: not enough space left in ringbuffer\n");
                    break;
                }

                int numConsumed = mStreamInfo->numTotalBytes;
                decoderErr = aacDecoder_DecodeFrame(mAACDecoder,
                                           tmpOutBuffer,
                                           2048 * MAX_CHANNEL_COUNT,
                                           0 /* flags */);

                numConsumed = mStreamInfo->numTotalBytes - numConsumed;
                numLoops++;

                if (decoderErr == AAC_DEC_NOT_ENOUGH_BITS) {
                    break;
                }
                mDecodedSizes.push_back(numConsumed);

                if (decoderErr != AAC_DEC_OK) {
                    logw("aacDecoder_DecodeFrame decoderErr = 0x%4.4x\n", decoderErr);
                }

                if (bytesValid[0] != 0) {
                    loge("bytesValid[0] != 0 should never happen\n");
                    mSignalledError = true;
                    notify(OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                    return;
                }

                size_t numOutBytes =
                    mStreamInfo->frameSize * sizeof(int16_t) * mStreamInfo->numChannels;

                if (decoderErr == AAC_DEC_OK) {
                    if (!outputDelayRingBufferPutSamples(tmpOutBuffer,
                            mStreamInfo->frameSize * mStreamInfo->numChannels)) {
                        mSignalledError = true;
                        notify(OMX_EventError, OMX_ErrorUndefined, decoderErr, NULL);
                        return;
                    }
                } else {
                    logw("AAC decoder returned error 0x%4.4x, substituting silence\n", decoderErr);

                    memset(tmpOutBuffer, 0, numOutBytes); // TODO: check for overflow

                    if (!outputDelayRingBufferPutSamples(tmpOutBuffer,
                            mStreamInfo->frameSize * mStreamInfo->numChannels)) {
                        mSignalledError = true;
                        notify(OMX_EventError, OMX_ErrorUndefined, decoderErr, NULL);
                        return;
                    }

                    // Discard input buffer.
                    if (inHeader) {
                        inHeader->nFilledLen = 0;
                    }

                    aacDecoder_SetParam(mAACDecoder, AAC_TPDEC_CLEAR_BUFFER, 1);

                    // After an error, replace the last entry in mBufferSizes with the sum of the
                    // last <numLoops> entries from mDecodedSizes to resynchronize the in/out lists.
                    mBufferSizes.pop_back(); //pop
                    int n = 0;
                    for (int i = 0; i < numLoops; i++) {
                        n += mDecodedSizes.at(mDecodedSizes.size() - numLoops + i);
                    }
                    mBufferSizes.push_back(n);

                    // fall through
                }

                /*
                 * AAC+/eAAC+ streams can be signalled in two ways: either explicitly
                 * or implicitly, according to MPEG4 spec. AAC+/eAAC+ is a dual
                 * rate system and the sampling rate in the final output is actually
                 * doubled compared with the core AAC decoder sampling rate.
                 *
                 * Explicit signalling is done by explicitly defining SBR audio object
                 * type in the bitstream. Implicit signalling is done by embedding
                 * SBR content in AAC extension payload specific to SBR, and hence
                 * requires an AAC decoder to perform pre-checks on actual audio frames.
                 *
                 * Thus, we could not say for sure whether a stream is
                 * AAC+/eAAC+ until the first data frame is decoded.
                 */
                if (mInputBufferCount <= 2 || mOutputBufferCount > 1) { // TODO: <= 1
                    if (mStreamInfo->sampleRate != prevSampleRate ||
                        mStreamInfo->numChannels != prevNumChannels) {
                        logi("Reconfiguring decoder: %d->%d Hz, %d->%d channels\n",
                              prevSampleRate, mStreamInfo->sampleRate,
                              prevNumChannels, mStreamInfo->numChannels);

                        notify(OMX_EventPortSettingsChanged, 1, 0, NULL);
                        mOutputPortSettingsChange = NONE;//AWAITING_DISABLED; modify by yu

                        if (inHeader && inHeader->nFilledLen == 0) {
                            inInfo->mOwnedByUs = false;
                            mInputBufferCount++;

                            //During Port reconfiguration current frames is skipped and next frame
                            //is sent for decoding.
                            //Update mNextOutBufferTimeUs with current frame's timestamp if port reconfiguration is
                            //happening in last frame of current buffer otherwise LastOutBufferTimeUs
                            //will be zero(post seek).
                            mNextOutBufferTimeUs = mBufferTimestamps.at(mBufferTimestamps.size() - 1)
                                                  + mStreamInfo->aacSamplesPerFrame *
                                                       1000000ll / mStreamInfo->aacSampleRate;
                            inQueue.erase(inQueue.begin());
                            mLastInHeader = NULL;
                            inInfo = NULL;
                            notifyEmptyBufferDone(inHeader);
                            inHeader = NULL;
                        }
                        return;
                    }
                } else if (!mStreamInfo->sampleRate || !mStreamInfo->numChannels) {
                    logw("Invalid AAC stream\n");
                    mSignalledError = true;
                    notify(OMX_EventError, OMX_ErrorUndefined, decoderErr, NULL);
                    return;
                }
                if (inHeader && inHeader->nFilledLen == 0) {
                    inInfo->mOwnedByUs = false;
                    mInputBufferCount++;
                    inQueue.erase(inQueue.begin());
                    mLastInHeader = NULL;
                    inInfo = NULL;
                    notifyEmptyBufferDone(inHeader);
                    inHeader = NULL;
                } else {
                    logv("inHeader->nFilledLen = %d\n", inHeader ? inHeader->nFilledLen : 0);
                }
            } while (decoderErr == AAC_DEC_OK);
        }

        int32_t outputDelay = mStreamInfo->outputDelay * mStreamInfo->numChannels;

        if (!mEndOfInput && mOutputDelayCompensated < outputDelay) {
            // discard outputDelay at the beginning
            int32_t toCompensate = outputDelay - mOutputDelayCompensated;
            int32_t discard = outputDelayRingBufferSamplesAvailable();
            if (discard > toCompensate) {
                discard = toCompensate;
            }
            int32_t discarded = outputDelayRingBufferGetSamples(0, discard);
            mOutputDelayCompensated += discarded;
            continue;
        }

        if (mEndOfInput) {
            while (mOutputDelayCompensated > 0) {
                // a buffer big enough for MAX_CHANNEL_COUNT channels of decoded HE-AAC
                INT_PCM tmpOutBuffer[2048 * MAX_CHANNEL_COUNT];

                 // run DRC check
                 mDrcWrap.submitStreamData(mStreamInfo);
                 mDrcWrap.update();

                AAC_DECODER_ERROR decoderErr =
                    aacDecoder_DecodeFrame(mAACDecoder,
                                           tmpOutBuffer,
                                           2048 * MAX_CHANNEL_COUNT,
                                           AACDEC_FLUSH);
                if (decoderErr != AAC_DEC_OK) {
                    logw("aacDecoder_DecodeFrame decoderErr = 0x%4.4x\n", decoderErr);
                }

                int32_t tmpOutBufferSamples = mStreamInfo->frameSize * mStreamInfo->numChannels;
                if (tmpOutBufferSamples > mOutputDelayCompensated) {
                    tmpOutBufferSamples = mOutputDelayCompensated;
                }
                outputDelayRingBufferPutSamples(tmpOutBuffer, tmpOutBufferSamples);
                mOutputDelayCompensated -= tmpOutBufferSamples;
            }
        }

        while (!outQueue.empty()
                && outputDelayRingBufferSamplesAvailable()
                        >= mStreamInfo->frameSize * mStreamInfo->numChannels) {
            BufferInfo *outInfo = *outQueue.begin();
            OMX_BUFFERHEADERTYPE *outHeader = outInfo->mHeader;

            if (outHeader->nOffset != 0) {
                loge("outHeader->nOffset != 0 is not handled\n");
                mSignalledError = true;
                notify(OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                return;
            }

            INT_PCM *outBuffer =
                    reinterpret_cast<INT_PCM *>(outHeader->pBuffer + outHeader->nOffset);
            int samplesize = mStreamInfo->numChannels * sizeof(int16_t);
            if (outHeader->nOffset
                    + mStreamInfo->frameSize * samplesize
                    > outHeader->nAllocLen) {
                loge("buffer overflow\n");
                mSignalledError = true;
                notify(OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                return;

            }

            int available = outputDelayRingBufferSamplesAvailable();
            int numSamples = outHeader->nAllocLen / sizeof(int16_t);
            if (numSamples > available) {
                numSamples = available;
            }
            int64_t currentTime = 0;
            if (available) {

                int numFrames = numSamples / (mStreamInfo->frameSize * mStreamInfo->numChannels);
                numSamples = numFrames * (mStreamInfo->frameSize * mStreamInfo->numChannels);

                logv("%d samples available (%d), or %d frames\n",
                        numSamples, available, numFrames);
                int64_t *nextTimeStamp = &mBufferTimestamps.at(0);
                currentTime = *nextTimeStamp;
                int32_t *currentBufLeft = &mBufferSizes.at(0);
                for (int i = 0; i < numFrames; i++) {
                    int32_t decodedSize = mDecodedSizes.at(0);
                    mDecodedSizes.erase(mDecodedSizes.begin());
                    logv("decoded %d of %d\n", decodedSize, *currentBufLeft);
                    if (*currentBufLeft > decodedSize) {
                        // adjust/interpolate next time stamp
                        *currentBufLeft -= decodedSize;
                        *nextTimeStamp += mStreamInfo->aacSamplesPerFrame *
                                1000000ll / mStreamInfo->aacSampleRate;
                        mNextOutBufferTimeUs = *nextTimeStamp;
                        logv("adjusted nextTimeStamp/size to %lld/%d\n",
                                (long long) *nextTimeStamp, *currentBufLeft);
                    } else {
                        // move to next timestamp in list
                        if (mBufferTimestamps.size() > 0) {
                            mBufferTimestamps.erase(mBufferTimestamps.begin());
                            nextTimeStamp = &mBufferTimestamps.at(0);
                            mNextOutBufferTimeUs = *nextTimeStamp;
                            mBufferSizes.erase(mBufferSizes.begin());
                            currentBufLeft = &mBufferSizes.at(0);
                            logv("moved to next time/size: %lld/%d\n",
                                    (long long) *nextTimeStamp, *currentBufLeft);
                        }
                        // try to limit output buffer size to match input buffers
                        // (e.g when an input buffer contained 4 "sub" frames, output
                        // at most 4 decoded units in the corresponding output buffer)
                        // This is optional. Remove the next three lines to fill the output
                        // buffer with as many units as available.
                        numFrames = i + 1;
                        numSamples = numFrames * mStreamInfo->frameSize * mStreamInfo->numChannels;
                        break;
                    }
                }

                logv("getting %d from ringbuffer\n", numSamples);
                int32_t ns = outputDelayRingBufferGetSamples(outBuffer, numSamples);
                if (ns != numSamples) {
                    loge("not a complete frame of samples available\n");
                    mSignalledError = true;
                    notify(OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                    return;
                }
            }

            outHeader->nFilledLen = numSamples * sizeof(int16_t);

            if (mEndOfInput && !outQueue.empty() && outputDelayRingBufferSamplesAvailable() == 0) {
                outHeader->nFlags = OMX_BUFFERFLAG_EOS;
                mEndOfOutput = true;
            } else {
                outHeader->nFlags = 0;
            }

            outHeader->nTimeStamp = currentTime;

            mOutputBufferCount++;
            outInfo->mOwnedByUs = false;
            outQueue.erase(outQueue.begin());
            outInfo = NULL;
            logv("out timestamp %lld / %d\n", outHeader->nTimeStamp, outHeader->nFilledLen);
            notifyFillBufferDone(outHeader);
            outHeader = NULL;
        }

        if (mEndOfInput) {
            int ringBufAvail = outputDelayRingBufferSamplesAvailable();
            if (!outQueue.empty()
                    && ringBufAvail < mStreamInfo->frameSize * mStreamInfo->numChannels) {
                if (!mEndOfOutput) {
                    // send partial or empty block signaling EOS
                    mEndOfOutput = true;
                    BufferInfo *outInfo = *outQueue.begin();
                    OMX_BUFFERHEADERTYPE *outHeader = outInfo->mHeader;

                    INT_PCM *outBuffer = reinterpret_cast<INT_PCM *>(outHeader->pBuffer
                            + outHeader->nOffset);
                    int32_t ns = outputDelayRingBufferGetSamples(outBuffer, ringBufAvail);
                    if (ns < 0) {
                        ns = 0;
                    }
                    outHeader->nFilledLen = ns;
                    outHeader->nFlags = OMX_BUFFERFLAG_EOS;

                    outHeader->nTimeStamp = mBufferTimestamps.at(0);
                    mBufferTimestamps.clear();
                    mBufferSizes.clear();
                    mDecodedSizes.clear();

                    mOutputBufferCount++;
                    outInfo->mOwnedByUs = false;
                    outQueue.erase(outQueue.begin());
                    outInfo = NULL;
                    notifyFillBufferDone(outHeader);
                    outHeader = NULL;
                }
                break; // if outQueue not empty but no more output
            }
        }
    }
}

void SoftAACDecoder::onPortFlushCompleted(OMX_U32 portIndex) {
    if (portIndex == 0) {
        // Make sure that the next buffer output does not still
        // depend on fragments from the last one decoded.
        // drain all existing data
        drainDecoder();
        mBufferTimestamps.clear();
        mBufferSizes.clear();
        mDecodedSizes.clear();
        mLastInHeader = NULL;
        mEndOfInput = false;
        mLastHeaderTimeUs = -1;
        mNextOutBufferTimeUs = 0;
    } else {
        int avail;
        while ((avail = outputDelayRingBufferSamplesAvailable()) > 0) {
            if (avail > mStreamInfo->frameSize * mStreamInfo->numChannels) {
                avail = mStreamInfo->frameSize * mStreamInfo->numChannels;
            }
            int32_t ns = outputDelayRingBufferGetSamples(0, avail);
            if (ns != avail) {
                logw("not a complete frame of samples available\n");
                break;
            }
            mOutputBufferCount++;
        }
        mOutputDelayRingBufferReadPos = mOutputDelayRingBufferWritePos;
        mEndOfOutput = false;
    }
}

void SoftAACDecoder::drainDecoder() {
    // flush decoder until outputDelay is compensated
    while (mOutputDelayCompensated > 0) {
        // a buffer big enough for MAX_CHANNEL_COUNT channels of decoded HE-AAC
        INT_PCM tmpOutBuffer[2048 * MAX_CHANNEL_COUNT];

        // run DRC check
        mDrcWrap.submitStreamData(mStreamInfo);
        mDrcWrap.update();

        AAC_DECODER_ERROR decoderErr =
            aacDecoder_DecodeFrame(mAACDecoder,
                                   tmpOutBuffer,
                                   2048 * MAX_CHANNEL_COUNT,
                                   AACDEC_FLUSH);
        if (decoderErr != AAC_DEC_OK) {
            logw("aacDecoder_DecodeFrame decoderErr = 0x%4.4x\n", decoderErr);
        }

        int32_t tmpOutBufferSamples = mStreamInfo->frameSize * mStreamInfo->numChannels;
        if (tmpOutBufferSamples > mOutputDelayCompensated) {
            tmpOutBufferSamples = mOutputDelayCompensated;
        }
        outputDelayRingBufferPutSamples(tmpOutBuffer, tmpOutBufferSamples);

        mOutputDelayCompensated -= tmpOutBufferSamples;
    }
}

void SoftAACDecoder::onReset() {
    drainDecoder();
    // reset the "configured" state
    mInputBufferCount = 0;
    mOutputBufferCount = 0;
    mOutputDelayCompensated = 0;
    mOutputDelayRingBufferWritePos = 0;
    mOutputDelayRingBufferReadPos = 0;
    mOutputDelayRingBufferFilled = 0;
    mEndOfInput = false;
    mEndOfOutput = false;
    mBufferTimestamps.clear();
    mBufferSizes.clear();
    mDecodedSizes.clear();
    mLastInHeader = NULL;
    mLastHeaderTimeUs = -1;
    mNextOutBufferTimeUs = 0;

    // To make the codec behave the same before and after a reset, we need to invalidate the
    // streaminfo struct. This does that:
    mStreamInfo->sampleRate = 0; // TODO: mStreamInfo is read only

    mSignalledError = false;
    mOutputPortSettingsChange = NONE;
}

void SoftAACDecoder::onPortEnableCompleted(OMX_U32 portIndex, bool enabled) {
    if (portIndex != 1) {
        return;
    }

    switch (mOutputPortSettingsChange) {
        case NONE:
            break;

        case AWAITING_DISABLED:
        {
            CHECK(!enabled);
            mOutputPortSettingsChange = AWAITING_ENABLED;
            break;
        }

        default:
        {
            CHECK_EQ((int)mOutputPortSettingsChange, (int)AWAITING_ENABLED);
            CHECK(enabled);
            mOutputPortSettingsChange = NONE;
            break;
        }
    }
}

}  // namespace omxil
