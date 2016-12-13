#ifndef SOFT_H264_DEC_H_
#define SOFT_H264_DEC_H_

#include <malloc.h>
#include <utility_common.h>
#include <core_constructor.h>
#include "simple_soft_omx_component.h"
#include <sys/time.h>
#include "ih264_typedefs.h"
#include "iv.h"
#include "ivd.h"
#include "ih264d.h"

//#define FILE_DUMP_ENABLE

namespace omxil {

#define ivd_aligned_malloc(alignment, size) memalign(alignment, size)
#define ivd_aligned_free(buf) free(buf)

/** Number of entries in the time-stamp array */
#define MAX_TIME_STAMPS 64

/** Maximum number of cores supported by the codec */
#define CODEC_MAX_NUM_CORES 4

#define CODEC_MAX_WIDTH     1920

#define CODEC_MAX_HEIGHT    1088

/** Input buffer size */
#define INPUT_BUF_SIZE (1024 * 1024)

/** Used to remove warnings about unused parameters */
#define UNUSED(x) ((void)(x))

/** Compute difference between start and end */
#define TIME_DIFF(start, end, diff) \
    diff = (end - start)

struct CodecProfileLevel {
  OMX_U32 mProfile;
  OMX_U32 mLevel;
};

struct SoftAVCDec : public SimpleSoftOMXComponent {
    SoftAVCDec(const char *name, const OMX_CALLBACKTYPE *callbacks,
            OMX_PTR appData, OMX_COMPONENTTYPE **component);
protected:
    virtual ~SoftAVCDec();

    virtual void onQueueFilled(OMX_U32 portIndex);
    virtual void onPortFlushCompleted(OMX_U32 portIndex);
    virtual void onReset();
    virtual OMX_ERRORTYPE internalSetParameter(OMX_INDEXTYPE index, const OMX_PTR params);

    virtual OMX_ERRORTYPE internalGetParameter(
            OMX_INDEXTYPE index, OMX_PTR params);

    virtual OMX_ERRORTYPE getConfig(
            OMX_INDEXTYPE index, OMX_PTR params);

private:
    // Number of input and output buffers
    enum {
        kNumBuffers = 8
    };

    iv_obj_t *mCodecCtx;         // Codec context
    iv_mem_rec_t *mMemRecords;   // Memory records requested by the codec
    size_t mNumMemRecords;       // Number of memory records requested by the codec

    size_t mNumCores;            // Number of cores to be uesd by the codec

    int64_t mTimeStart;   // Time at the start of decode()
    int64_t mTimeEnd;     // Time at the end of decode()

    // Internal buffer to be used to flush out the buffers from decoder
    uint8_t *mFlushOutBuffer;

    // Status of entries in the timestamp array
    bool mTimeStampsValid[MAX_TIME_STAMPS];

    // Timestamp array - Since codec does not take 64 bit timestamps,
    // they are maintained in the plugin
    OMX_S64 mTimeStamps[MAX_TIME_STAMPS];

#ifdef FILE_DUMP_ENABLE
    char mInFile[200];
#endif /* FILE_DUMP_ENABLE */

    OMX_COLOR_FORMATTYPE mOmxColorFormat;    // OMX Color format
    IV_COLOR_FORMAT_T mIvColorFormat;        // Ittiam Color format

    bool mIsInFlush;        // codec is flush mode
    bool mReceivedEOS;      // EOS is receieved on input port
    bool mInitNeeded;
    uint32_t mNewWidth;
    uint32_t mNewHeight;
    uint32_t mNewLevel;
    // The input stream has changed to a different resolution, which is still supported by the
    // codec. So the codec is switching to decode the new resolution.
    bool mChangingResolution;
    bool mFlushNeeded;
    bool mSignalledError;

    void initPorts(OMX_U32 numInputBuffers,
        OMX_U32 numOutputBuffers);


    int32_t initDecoder(uint32_t width, uint32_t height);
    int32_t deInitDecoder();
    int32_t setFlushMode();
    int32_t setParams(size_t stride);
    void logVersion();
    int32_t setNumCores();
    int32_t resetDecoder();
    int32_t resetPlugin();
    int32_t reInitDecoder(uint32_t width, uint32_t height);

    void setDecodeArgs(
            ivd_video_decode_ip_t *ps_dec_ip,
            ivd_video_decode_op_t *ps_dec_op,
            OMX_BUFFERHEADERTYPE *inHeader,
            OMX_BUFFERHEADERTYPE *outHeader,
            size_t timeStampIx);

    uint32_t outputBufferWidth();
    uint32_t outputBufferHeight();
    void updatePortDefinitions(bool updateCrop = true, bool updateInputSize = false);

    enum CropSettingsMode {
        kCropUnSet = 0,
        kCropSet,
        kCropChanged,
    };
    void handlePortSettingsChange(
            bool *portWillReset, uint32_t width, uint32_t height,
            CropSettingsMode cropSettingsMode = kCropUnSet, bool fakeStride = false);


    uint32_t mWidth;      // width of the input frames
    uint32_t mHeight;     // height of the input frames
    enum {
        kInputPortIndex  = 0,
        kOutputPortIndex = 1,
        kMaxPortIndex = 1,
    };
    enum {
        NONE,
        AWAITING_DISABLED,
        AWAITING_ENABLED
    } mOutputPortSettingsChange;
    bool mIsAdaptive;
    uint32_t mAdaptiveMaxWidth, mAdaptiveMaxHeight;
    uint32_t mCropLeft, mCropTop, mCropWidth, mCropHeight;
    uint32_t mMinInputBufferSize;
    uint32_t mMinCompressionRatio;

    const char *mComponentRole;
    OMX_VIDEO_CODINGTYPE mCodingType;

    DISALLOW_EVIL_CONSTRUCTORS(SoftAVCDec);
};

#ifdef FILE_DUMP_ENABLE

#define INPUT_DUMP_PATH     "./avcd_input"
#define INPUT_DUMP_EXT      "h264"

#define GENERATE_FILE_NAMES() {                         \
    mTimeStart = os_get_systime();                      \
    strcpy(mInFile, "");                                \
    sprintf(mInFile, "%s_%ld.%s\n", INPUT_DUMP_PATH,     \
            mTimeStart,                                 \
            INPUT_DUMP_EXT);                            \
}

#define CREATE_DUMP_FILE(m_filename) {                  \
    FILE *fp = fopen(m_filename, "wb");                 \
    if (fp != NULL) {                                   \
        fclose(fp);                                     \
    } else {                                            \
        logv("Could not open file %s\n", m_filename);    \
    }                                                   \
}
#define DUMP_TO_FILE(m_filename, m_buf, m_size)         \
{                                                       \
    FILE *fp = fopen(m_filename, "ab");                 \
    if (fp != NULL && m_buf != NULL) {                  \
        int i;                                          \
        i = fwrite(m_buf, 1, m_size, fp);               \
        logv("fwrite ret %d to write %d\n", i, m_size);  \
        if (i != (int) m_size) {                        \
            logv("Error in fwrite, returned %d\n", i);   \
            perror("Error in write to file");           \
        }                                               \
        fclose(fp);                                     \
    } else {                                            \
        logv("Could not write to file %s\n", m_filename);\
    }                                                   \
}
#else /* FILE_DUMP_ENABLE */
#define INPUT_DUMP_PATH
#define INPUT_DUMP_EXT
#define OUTPUT_DUMP_PATH
#define OUTPUT_DUMP_EXT
#define GENERATE_FILE_NAMES()
#define CREATE_DUMP_FILE(m_filename)
#define DUMP_TO_FILE(m_filename, m_buf, m_size)
#endif /* FILE_DUMP_ENABLE */

} // namespace omxil

#endif  // SOFT_H264_DEC_H_
