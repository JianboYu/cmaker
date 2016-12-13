#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG "SoftAVCDec"

#include <list>
#include <string.h>
#include <unistd.h>
#include <os_typedefs.h>
#include <os_log.h>
#include <os_assert.h>
#include <os_time.h>
#include "OMX_Video.h"
#include "soft_avc_dec.h"

namespace omxil {

#define NO_MEMORY (-1)

const char *MEDIA_MIMETYPE_VIDEO_AVC = "video/avc";

/** Function and structure definitions to keep code similar for each codec */
#define ivdec_api_function              ih264d_api_function
#define ivdext_init_ip_t                ih264d_init_ip_t
#define ivdext_init_op_t                ih264d_init_op_t
#define ivdext_fill_mem_rec_ip_t        ih264d_fill_mem_rec_ip_t
#define ivdext_fill_mem_rec_op_t        ih264d_fill_mem_rec_op_t
#define ivdext_ctl_set_num_cores_ip_t   ih264d_ctl_set_num_cores_ip_t
#define ivdext_ctl_set_num_cores_op_t   ih264d_ctl_set_num_cores_op_t

#define IVDEXT_CMD_CTL_SET_NUM_CORES    \
        (IVD_CONTROL_API_COMMAND_TYPE_T)IH264D_CMD_CTL_SET_NUM_CORES

static const CodecProfileLevel kProfileLevels[] = {
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1b },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel11 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel12 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel13 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel2  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel21 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel22 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel3  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel31 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel32 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel4  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel41 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel42 },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel5  },
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel51 },
    //{ OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel52 },

    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel1  },
    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel1b },
    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel11 },
    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel12 },
    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel13 },
    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel2  },
    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel21 },
    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel22 },
    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel3  },
    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel31 },
    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel32 },
    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel4  },
    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel41 },
    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel42 },
    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel5  },
    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel51 },
    //{ OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel52 },

    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel1  },
    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel1b },
    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel11 },
    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel12 },
    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel13 },
    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel2  },
    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel21 },
    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel22 },
    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel3  },
    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel31 },
    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel32 },
    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel4  },
    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel41 },
    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel42 },
    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel5  },
    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel51 },
    //{ OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel52 },
};

template<class T>
static void InitOMXParams(T *params) {
    params->nSize = sizeof(T);
    params->nVersion.s.nVersionMajor = 1;
    params->nVersion.s.nVersionMinor = 0;
    params->nVersion.s.nRevision = 0;
    params->nVersion.s.nStep = 0;
}

SoftAVCDec::SoftAVCDec(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component)
    : SimpleSoftOMXComponent(
            name,
            callbacks,
            appData, component),
      mCodecCtx(NULL),
      mMemRecords(NULL),
      mFlushOutBuffer(NULL),
      mOmxColorFormat(OMX_COLOR_FormatYUV420Planar),
      mIvColorFormat(IV_YUV_420P),
      mNewLevel(0),
      mChangingResolution(false),
      mSignalledError(false) {
    mWidth = 320;
    mHeight = 240;
    mCropLeft = 0;
    mCropTop = 0;
    mCropWidth = mWidth;
    mCropHeight = mHeight;
    mNewWidth = mWidth;
    mNewHeight = mHeight;

    mAdaptiveMaxWidth = 0;
    mAdaptiveMaxHeight = 0;
    mMinInputBufferSize = INPUT_BUF_SIZE;
    mMinCompressionRatio = 1;

    mComponentRole = "video_decoder.avc";
    mCodingType = OMX_VIDEO_CodingAVC;

    initPorts(kNumBuffers, kNumBuffers);

    mTimeStart = os_get_systime();

    // If input dump is enabled, then open create an empty file
    GENERATE_FILE_NAMES();
    CREATE_DUMP_FILE(mInFile);

    CHECK_EQ(initDecoder(mWidth, mHeight), (int32_t)0);
}

SoftAVCDec::~SoftAVCDec() {
    CHECK_EQ(deInitDecoder(), (int32_t)0);
}

static size_t GetCPUCoreCount() {
    long cpuCoreCount = 1;
#if defined(_SC_NPROCESSORS_ONLN)
    cpuCoreCount = sysconf(_SC_NPROCESSORS_ONLN);
#else
    // _SC_NPROC_ONLN must be defined...
    cpuCoreCount = sysconf(_SC_NPROC_ONLN);
#endif
    CHECK(cpuCoreCount >= 1);
    logv("Number of CPU cores: %ld\n", cpuCoreCount);
    return (size_t)cpuCoreCount;
}

void SoftAVCDec::logVersion() {
    ivd_ctl_getversioninfo_ip_t s_ctl_ip;
    ivd_ctl_getversioninfo_op_t s_ctl_op;
    UWORD8 au1_buf[512];
    IV_API_CALL_STATUS_T status;

    s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
    s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_GETVERSION;
    s_ctl_ip.u4_size = sizeof(ivd_ctl_getversioninfo_ip_t);
    s_ctl_op.u4_size = sizeof(ivd_ctl_getversioninfo_op_t);
    s_ctl_ip.pv_version_buffer = au1_buf;
    s_ctl_ip.u4_version_buffer_size = sizeof(au1_buf);

    status =
        ivdec_api_function(mCodecCtx, (void *)&s_ctl_ip, (void *)&s_ctl_op);

    if (status != IV_SUCCESS) {
        loge("Error in getting version number: 0x%x",
                s_ctl_op.u4_error_code);
    } else {
        logv("Ittiam decoder version number: %s",
                (char *)s_ctl_ip.pv_version_buffer);
    }
    return;
}

int32_t SoftAVCDec::setParams(size_t stride) {
    ivd_ctl_set_config_ip_t s_ctl_ip;
    ivd_ctl_set_config_op_t s_ctl_op;
    IV_API_CALL_STATUS_T status;
    s_ctl_ip.u4_disp_wd = (UWORD32)stride;
    s_ctl_ip.e_frm_skip_mode = IVD_SKIP_NONE;

    s_ctl_ip.e_frm_out_mode = IVD_DISPLAY_FRAME_OUT;
    s_ctl_ip.e_vid_dec_mode = IVD_DECODE_FRAME;
    s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
    s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_SETPARAMS;
    s_ctl_ip.u4_size = sizeof(ivd_ctl_set_config_ip_t);
    s_ctl_op.u4_size = sizeof(ivd_ctl_set_config_op_t);

    logv("Set the run-time (dynamic) parameters stride = %zu", stride);
    status = ivdec_api_function(mCodecCtx, (void *)&s_ctl_ip, (void *)&s_ctl_op);

    if (status != IV_SUCCESS) {
        loge("Error in setting the run-time parameters: 0x%x",
                s_ctl_op.u4_error_code);

        return -1;
    }
    return 0;
}

int32_t SoftAVCDec::resetPlugin() {
    mIsInFlush = false;
    mReceivedEOS = false;
    memset(mTimeStamps, 0, sizeof(mTimeStamps));
    memset(mTimeStampsValid, 0, sizeof(mTimeStampsValid));

    /* Initialize both start and end times */
    mTimeStart = os_get_systime();
    mTimeEnd = os_get_systime();

    return 0;
}

int32_t SoftAVCDec::resetDecoder() {
    ivd_ctl_reset_ip_t s_ctl_ip;
    ivd_ctl_reset_op_t s_ctl_op;
    IV_API_CALL_STATUS_T status;

    s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
    s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_RESET;
    s_ctl_ip.u4_size = sizeof(ivd_ctl_reset_ip_t);
    s_ctl_op.u4_size = sizeof(ivd_ctl_reset_op_t);

    status = ivdec_api_function(mCodecCtx, (void *)&s_ctl_ip, (void *)&s_ctl_op);
    if (IV_SUCCESS != status) {
        loge("Error in reset: 0x%x", s_ctl_op.u4_error_code);
        return -1;
    }
    mSignalledError = false;

    /* Set the run-time (dynamic) parameters */
    setParams(outputBufferWidth());

    /* Set number of cores/threads to be used by the codec */
    setNumCores();

    return 0;
}

int32_t SoftAVCDec::setNumCores() {
    ivdext_ctl_set_num_cores_ip_t s_set_cores_ip;
    ivdext_ctl_set_num_cores_op_t s_set_cores_op;
    IV_API_CALL_STATUS_T status;
    s_set_cores_ip.e_cmd = IVD_CMD_VIDEO_CTL;
    s_set_cores_ip.e_sub_cmd = IVDEXT_CMD_CTL_SET_NUM_CORES;
    s_set_cores_ip.u4_num_cores = MIN(mNumCores, CODEC_MAX_NUM_CORES);
    s_set_cores_ip.u4_size = sizeof(ivdext_ctl_set_num_cores_ip_t);
    s_set_cores_op.u4_size = sizeof(ivdext_ctl_set_num_cores_op_t);
    status = ivdec_api_function(
            mCodecCtx, (void *)&s_set_cores_ip, (void *)&s_set_cores_op);
    if (IV_SUCCESS != status) {
        loge("Error in setting number of cores: 0x%x",
                s_set_cores_op.u4_error_code);
        return -1;
    }
    return 0;
}

int32_t SoftAVCDec::setFlushMode() {
    IV_API_CALL_STATUS_T status;
    ivd_ctl_flush_ip_t s_video_flush_ip;
    ivd_ctl_flush_op_t s_video_flush_op;

    s_video_flush_ip.e_cmd = IVD_CMD_VIDEO_CTL;
    s_video_flush_ip.e_sub_cmd = IVD_CMD_CTL_FLUSH;
    s_video_flush_ip.u4_size = sizeof(ivd_ctl_flush_ip_t);
    s_video_flush_op.u4_size = sizeof(ivd_ctl_flush_op_t);

    /* Set the decoder in Flush mode, subsequent decode() calls will flush */
    status = ivdec_api_function(
            mCodecCtx, (void *)&s_video_flush_ip, (void *)&s_video_flush_op);

    if (status != IV_SUCCESS) {
        loge("Error in setting the decoder in flush mode: (%d) 0x%x", status,
                s_video_flush_op.u4_error_code);
        return -1;
    }

    mIsInFlush = true;
    return 0;
}

int32_t SoftAVCDec::initDecoder(uint32_t width, uint32_t height) {
    IV_API_CALL_STATUS_T status;

    UWORD32 u4_num_reorder_frames;
    UWORD32 u4_num_ref_frames;
    UWORD32 u4_share_disp_buf;
    WORD32 i4_level;

    mNumCores = GetCPUCoreCount();
    mCodecCtx = NULL;

    /* Initialize number of ref and reorder modes (for H264) */
    u4_num_reorder_frames = 16;
    u4_num_ref_frames = 16;
    u4_share_disp_buf = 0;

    uint32_t displayStride = mIsAdaptive ? mAdaptiveMaxWidth : width;
    uint32_t displayHeight = mIsAdaptive ? mAdaptiveMaxHeight : height;
    uint32_t displaySizeY = displayStride * displayHeight;

    if(mNewLevel == 0){
        if (displaySizeY > (1920 * 1088)) {
            i4_level = 50;
        } else if (displaySizeY > (1280 * 720)) {
            i4_level = 40;
        } else if (displaySizeY > (720 * 576)) {
            i4_level = 31;
        } else if (displaySizeY > (624 * 320)) {
            i4_level = 30;
        } else if (displaySizeY > (352 * 288)) {
            i4_level = 21;
        } else {
            i4_level = 20;
        }
    } else {
        i4_level = mNewLevel;
    }

    {
        iv_num_mem_rec_ip_t s_num_mem_rec_ip;
        iv_num_mem_rec_op_t s_num_mem_rec_op;

        s_num_mem_rec_ip.u4_size = sizeof(s_num_mem_rec_ip);
        s_num_mem_rec_op.u4_size = sizeof(s_num_mem_rec_op);
        s_num_mem_rec_ip.e_cmd = IV_CMD_GET_NUM_MEM_REC;

        logv("Get number of mem records");
        status = ivdec_api_function(
                mCodecCtx, (void *)&s_num_mem_rec_ip, (void *)&s_num_mem_rec_op);
        if (IV_SUCCESS != status) {
            loge("Error in getting mem records: 0x%x",
                    s_num_mem_rec_op.u4_error_code);
            return -1;
        }

        mNumMemRecords = s_num_mem_rec_op.u4_num_mem_rec;
    }

    mMemRecords = (iv_mem_rec_t *)ivd_aligned_malloc(
            128, mNumMemRecords * sizeof(iv_mem_rec_t));
    if (mMemRecords == NULL) {
        loge("Allocation failure");
        return NO_MEMORY;
    }

    memset(mMemRecords, 0, mNumMemRecords * sizeof(iv_mem_rec_t));

    {
        size_t i;
        ivdext_fill_mem_rec_ip_t s_fill_mem_ip;
        ivdext_fill_mem_rec_op_t s_fill_mem_op;
        iv_mem_rec_t *ps_mem_rec;

        s_fill_mem_ip.s_ivd_fill_mem_rec_ip_t.u4_size =
            sizeof(ivdext_fill_mem_rec_ip_t);
        s_fill_mem_ip.i4_level = i4_level;
        s_fill_mem_ip.u4_num_reorder_frames = u4_num_reorder_frames;
        s_fill_mem_ip.u4_num_ref_frames = u4_num_ref_frames;
        s_fill_mem_ip.u4_share_disp_buf = u4_share_disp_buf;
        s_fill_mem_ip.u4_num_extra_disp_buf = 0;
        s_fill_mem_ip.e_output_format = mIvColorFormat;

        s_fill_mem_ip.s_ivd_fill_mem_rec_ip_t.e_cmd = IV_CMD_FILL_NUM_MEM_REC;
        s_fill_mem_ip.s_ivd_fill_mem_rec_ip_t.pv_mem_rec_location = mMemRecords;
        s_fill_mem_ip.s_ivd_fill_mem_rec_ip_t.u4_max_frm_wd = displayStride;
        s_fill_mem_ip.s_ivd_fill_mem_rec_ip_t.u4_max_frm_ht = displayHeight;
        s_fill_mem_op.s_ivd_fill_mem_rec_op_t.u4_size =
            sizeof(ivdext_fill_mem_rec_op_t);

        ps_mem_rec = mMemRecords;
        for (i = 0; i < mNumMemRecords; i++) {
            ps_mem_rec[i].u4_size = sizeof(iv_mem_rec_t);
        }

        status = ivdec_api_function(
                mCodecCtx, (void *)&s_fill_mem_ip, (void *)&s_fill_mem_op);

        if (IV_SUCCESS != status) {
            loge("Error in filling mem records: 0x%x",
                    s_fill_mem_op.s_ivd_fill_mem_rec_op_t.u4_error_code);
            return -1;
        }
        mNumMemRecords =
            s_fill_mem_op.s_ivd_fill_mem_rec_op_t.u4_num_mem_rec_filled;

        ps_mem_rec = mMemRecords;

        for (i = 0; i < mNumMemRecords; i++) {
            ps_mem_rec->pv_base = ivd_aligned_malloc(
                    ps_mem_rec->u4_mem_alignment, ps_mem_rec->u4_mem_size);
            if (ps_mem_rec->pv_base == NULL) {
                loge("Allocation failure for memory record #%zu of size %u",
                        i, ps_mem_rec->u4_mem_size);
                status = IV_FAIL;
                return NO_MEMORY;
            }

            ps_mem_rec++;
        }
    }

    /* Initialize the decoder */
    {
        ivdext_init_ip_t s_init_ip;
        ivdext_init_op_t s_init_op;

        void *dec_fxns = (void *)ivdec_api_function;

        s_init_ip.s_ivd_init_ip_t.u4_size = sizeof(ivdext_init_ip_t);
        s_init_ip.s_ivd_init_ip_t.e_cmd = (IVD_API_COMMAND_TYPE_T)IV_CMD_INIT;
        s_init_ip.s_ivd_init_ip_t.pv_mem_rec_location = mMemRecords;
        s_init_ip.s_ivd_init_ip_t.u4_frm_max_wd = displayStride;
        s_init_ip.s_ivd_init_ip_t.u4_frm_max_ht = displayHeight;

        s_init_ip.i4_level = i4_level;
        s_init_ip.u4_num_reorder_frames = u4_num_reorder_frames;
        s_init_ip.u4_num_ref_frames = u4_num_ref_frames;
        s_init_ip.u4_share_disp_buf = u4_share_disp_buf;
        s_init_ip.u4_num_extra_disp_buf = 0;

        s_init_op.s_ivd_init_op_t.u4_size = sizeof(s_init_op);

        s_init_ip.s_ivd_init_ip_t.u4_num_mem_rec = mNumMemRecords;
        s_init_ip.s_ivd_init_ip_t.e_output_format = mIvColorFormat;

        mCodecCtx = (iv_obj_t *)mMemRecords[0].pv_base;
        mCodecCtx->pv_fxns = dec_fxns;
        mCodecCtx->u4_size = sizeof(iv_obj_t);

        status = ivdec_api_function(mCodecCtx, (void *)&s_init_ip, (void *)&s_init_op);
        if (status != IV_SUCCESS) {
            mCodecCtx = NULL;
            loge("Error in init: 0x%x",
                    s_init_op.s_ivd_init_op_t.u4_error_code);
            return -1;
        }
    }

    /* Reset the plugin state */
    resetPlugin();

    /* Set the run time (dynamic) parameters */
    setParams(displayStride);

    /* Set number of cores/threads to be used by the codec */
    setNumCores();

    /* Get codec version */
    logVersion();

    /* Allocate internal picture buffer */
    uint32_t bufferSize = displaySizeY * 3 / 2;
    mFlushOutBuffer = (uint8_t *)ivd_aligned_malloc(128, bufferSize);
    if (NULL == mFlushOutBuffer) {
        loge("Could not allocate flushOutputBuffer of size %u", bufferSize);
        return NO_MEMORY;
    }

    mInitNeeded = false;
    mFlushNeeded = false;
    return 0;
}

int32_t SoftAVCDec::deInitDecoder() {
    size_t i;

    if (mMemRecords) {
        iv_mem_rec_t *ps_mem_rec;

        ps_mem_rec = mMemRecords;
        for (i = 0; i < mNumMemRecords; i++) {
            if (ps_mem_rec->pv_base) {
                ivd_aligned_free(ps_mem_rec->pv_base);
            }
            ps_mem_rec++;
        }
        ivd_aligned_free(mMemRecords);
        mMemRecords = NULL;
    }

    if (mFlushOutBuffer) {
        ivd_aligned_free(mFlushOutBuffer);
        mFlushOutBuffer = NULL;
    }

    mInitNeeded = true;
    mChangingResolution = false;

    return 0;
}

int32_t SoftAVCDec::reInitDecoder(uint32_t width, uint32_t height) {
    int32_t ret;

    deInitDecoder();

    ret = initDecoder(width, height);
    if (0 != ret) {
        loge("Create failure");
        deInitDecoder();
        return NO_MEMORY;
    }
    return 0;
}

void SoftAVCDec::onReset() {
    mOutputPortSettingsChange = NONE;
    mSignalledError = false;
    resetDecoder();
    resetPlugin();
}

OMX_ERRORTYPE SoftAVCDec::internalSetParameter(OMX_INDEXTYPE index, const OMX_PTR params) {
    const uint32_t oldWidth = mWidth;
    const uint32_t oldHeight = mHeight;
    const int32_t indexFull = index;

    switch (indexFull) {
        case OMX_IndexParamStandardComponentRole:
        {
            const OMX_PARAM_COMPONENTROLETYPE *roleParams =
                (const OMX_PARAM_COMPONENTROLETYPE *)params;

            if (strncmp((const char *)roleParams->cRole,
                        mComponentRole,
                        OMX_MAX_STRINGNAME_SIZE - 1)) {
                return OMX_ErrorUndefined;
            }

            break;
        }

        case OMX_IndexParamVideoPortFormat:
        {
            OMX_VIDEO_PARAM_PORTFORMATTYPE *formatParams =
                (OMX_VIDEO_PARAM_PORTFORMATTYPE *)params;

            if (formatParams->nPortIndex > kMaxPortIndex) {
                return OMX_ErrorBadPortIndex;
            }

            if (formatParams->nIndex != 0) {
                return OMX_ErrorNoMore;
            }

            if (formatParams->nPortIndex == kInputPortIndex) {
                if (formatParams->eCompressionFormat != mCodingType
                        || formatParams->eColorFormat != OMX_COLOR_FormatUnused) {
                    return OMX_ErrorUnsupportedSetting;
                }
            } else {
                if (formatParams->eCompressionFormat != OMX_VIDEO_CodingUnused
                        || formatParams->eColorFormat != OMX_COLOR_FormatYUV420Planar) {
                    return OMX_ErrorUnsupportedSetting;
                }
            }
            break;
        }
#if 0
        case kPrepareForAdaptivePlaybackIndex:
        {
            const PrepareForAdaptivePlaybackParams* adaptivePlaybackParams =
                    (const PrepareForAdaptivePlaybackParams *)params;
            mIsAdaptive = adaptivePlaybackParams->bEnable;
            if (mIsAdaptive) {
                mAdaptiveMaxWidth = adaptivePlaybackParams->nMaxFrameWidth;
                mAdaptiveMaxHeight = adaptivePlaybackParams->nMaxFrameHeight;
                mWidth = mAdaptiveMaxWidth;
                mHeight = mAdaptiveMaxHeight;
            } else {
                mAdaptiveMaxWidth = 0;
                mAdaptiveMaxHeight = 0;
            }
            updatePortDefinitions(true /* updateCrop */, true /* updateInputSize */);
            break;
        }
#endif
        case OMX_IndexParamPortDefinition:
        {
            OMX_PARAM_PORTDEFINITIONTYPE *newParams =
                (OMX_PARAM_PORTDEFINITIONTYPE *)params;
            OMX_VIDEO_PORTDEFINITIONTYPE *video_def = &newParams->format.video;
            OMX_PARAM_PORTDEFINITIONTYPE *def = &editPortInfo(newParams->nPortIndex)->mDef;

            uint32_t oldWidth = def->format.video.nFrameWidth;
            uint32_t oldHeight = def->format.video.nFrameHeight;
            uint32_t newWidth = video_def->nFrameWidth;
            uint32_t newHeight = video_def->nFrameHeight;
            if (newWidth != oldWidth || newHeight != oldHeight) {
                bool outputPort = (newParams->nPortIndex == kOutputPortIndex);
                if (outputPort) {
                    // only update (essentially crop) if size changes
                    mWidth = newWidth;
                    mHeight = newHeight;

                    updatePortDefinitions(true /* updateCrop */, true /* updateInputSize */);
                    // reset buffer size based on frame size
                    newParams->nBufferSize = def->nBufferSize;
                } else {
                    // For input port, we only set nFrameWidth and nFrameHeight. Buffer size
                    // is updated when configuring the output port using the max-frame-size,
                    // though client can still request a larger size.
                    def->format.video.nFrameWidth = newWidth;
                    def->format.video.nFrameHeight = newHeight;
                }
            }
            SimpleSoftOMXComponent::internalSetParameter(index, params);
            break;
        }
        default:
            SimpleSoftOMXComponent::internalSetParameter(index, params);
        break;
    }

    if (mWidth != oldWidth || mHeight != oldHeight) {
        mNewWidth = mWidth;
        mNewHeight = mHeight;
        int32_t err = reInitDecoder(mNewWidth, mNewHeight);
        if (err != 0) {
            notify(OMX_EventError, OMX_ErrorUnsupportedSetting, err, NULL);
            mSignalledError = true;
            return OMX_ErrorUnsupportedSetting;
        }
    }
    return OMX_ErrorNone;
}

void SoftAVCDec::setDecodeArgs(
        ivd_video_decode_ip_t *ps_dec_ip,
        ivd_video_decode_op_t *ps_dec_op,
        OMX_BUFFERHEADERTYPE *inHeader,
        OMX_BUFFERHEADERTYPE *outHeader,
        size_t timeStampIx) {
    size_t sizeY = outputBufferWidth() * outputBufferHeight();
    size_t sizeUV;
    uint8_t *pBuf;

    ps_dec_ip->u4_size = sizeof(ivd_video_decode_ip_t);
    ps_dec_op->u4_size = sizeof(ivd_video_decode_op_t);

    ps_dec_ip->e_cmd = IVD_CMD_VIDEO_DECODE;

    /* When in flush and after EOS with zero byte input,
     * inHeader is set to zero. Hence check for non-null */
    if (inHeader) {
        ps_dec_ip->u4_ts = timeStampIx;
        ps_dec_ip->pv_stream_buffer =
            inHeader->pBuffer + inHeader->nOffset;
        ps_dec_ip->u4_num_Bytes = inHeader->nFilledLen;
    } else {
        ps_dec_ip->u4_ts = 0;
        ps_dec_ip->pv_stream_buffer = NULL;
        ps_dec_ip->u4_num_Bytes = 0;
    }

    if (outHeader) {
        pBuf = outHeader->pBuffer;
    } else {
        pBuf = mFlushOutBuffer;
    }

    sizeUV = sizeY / 4;
    ps_dec_ip->s_out_buffer.u4_min_out_buf_size[0] = sizeY;
    ps_dec_ip->s_out_buffer.u4_min_out_buf_size[1] = sizeUV;
    ps_dec_ip->s_out_buffer.u4_min_out_buf_size[2] = sizeUV;

    ps_dec_ip->s_out_buffer.pu1_bufs[0] = pBuf;
    ps_dec_ip->s_out_buffer.pu1_bufs[1] = pBuf + sizeY;
    ps_dec_ip->s_out_buffer.pu1_bufs[2] = pBuf + sizeY + sizeUV;
    ps_dec_ip->s_out_buffer.u4_num_bufs = 3;
    return;
}
void SoftAVCDec::onPortFlushCompleted(OMX_U32 portIndex) {
    /* Once the output buffers are flushed, ignore any buffers that are held in decoder */
    if (kOutputPortIndex == portIndex) {
        setFlushMode();

        while (true) {
            ivd_video_decode_ip_t s_dec_ip;
            ivd_video_decode_op_t s_dec_op;
            IV_API_CALL_STATUS_T status __attribute((__unused__));
            //size_t sizeY, sizeUV;

            setDecodeArgs(&s_dec_ip, &s_dec_op, NULL, NULL, 0);

            status = ivdec_api_function(mCodecCtx, (void *)&s_dec_ip, (void *)&s_dec_op);
            if (0 == s_dec_op.u4_output_present) {
                resetPlugin();
                break;
            }
        }
    }
}

void SoftAVCDec::onQueueFilled(OMX_U32 portIndex) {
    UNUSED(portIndex);

    if (mSignalledError) {
        return;
    }
    if (mOutputPortSettingsChange != NONE) {
        return;
    }

    std::list<BufferInfo *> &inQueue = getPortQueue(kInputPortIndex);
    std::list<BufferInfo *> &outQueue = getPortQueue(kOutputPortIndex);

    /* If input EOS is seen and decoder is not in flush mode,
     * set the decoder in flush mode.
     * There can be a case where EOS is sent along with last picture data
     * In that case, only after decoding that input data, decoder has to be
     * put in flush. This case is handled here  */

    if (mReceivedEOS && !mIsInFlush) {
        setFlushMode();
    }

    while (!outQueue.empty()) {
        BufferInfo *inInfo;
        OMX_BUFFERHEADERTYPE *inHeader;

        BufferInfo *outInfo;
        OMX_BUFFERHEADERTYPE *outHeader;
        size_t timeStampIx;

        inInfo = NULL;
        inHeader = NULL;

        if (!mIsInFlush) {
            if (!inQueue.empty()) {
                inInfo = *inQueue.begin();
                inHeader = inInfo->mHeader;
                if (inHeader == NULL) {
                    inQueue.erase(inQueue.begin());
                    inInfo->mOwnedByUs = false;
                    continue;
                }
            } else {
                break;
            }
        }

        outInfo = *outQueue.begin();
        outHeader = outInfo->mHeader;
        outHeader->nFlags = 0;
        outHeader->nTimeStamp = 0;
        outHeader->nOffset = 0;

        if (inHeader != NULL) {
            if (inHeader->nFilledLen == 0) {
                inQueue.erase(inQueue.begin());
                inInfo->mOwnedByUs = false;
                notifyEmptyBufferDone(inHeader);

                if (!(inHeader->nFlags & OMX_BUFFERFLAG_EOS)) {
                    continue;
                }

                mReceivedEOS = true;
                inHeader = NULL;
                setFlushMode();
            } else if (inHeader->nFlags & OMX_BUFFERFLAG_EOS) {
                mReceivedEOS = true;
            }
        }

        // When there is an init required and the decoder is not in flush mode,
        // update output port's definition and reinitialize decoder.
        if (mInitNeeded && !mIsInFlush) {
            bool portWillReset = false;

            int32_t err = reInitDecoder(mNewWidth, mNewHeight);
            if (err != 0) {
                notify(OMX_EventError, OMX_ErrorUnsupportedSetting, err, NULL);
                mSignalledError = true;
                return;
            }

            handlePortSettingsChange(&portWillReset, mNewWidth, mNewHeight);
            return;
        }

        /* Get a free slot in timestamp array to hold input timestamp */
        {
            size_t i;
            timeStampIx = 0;
            for (i = 0; i < MAX_TIME_STAMPS; i++) {
                if (!mTimeStampsValid[i]) {
                    timeStampIx = i;
                    break;
                }
            }
            if (inHeader != NULL) {
                mTimeStampsValid[timeStampIx] = true;
                mTimeStamps[timeStampIx] = inHeader->nTimeStamp;
            }
        }

        {
            ivd_video_decode_ip_t s_dec_ip;
            ivd_video_decode_op_t s_dec_op;
            WORD32 timeDelay, timeTaken;
            //size_t sizeY, sizeUV;

            setDecodeArgs(&s_dec_ip, &s_dec_op, inHeader, outHeader, timeStampIx);
            // If input dump is enabled, then write to file
            DUMP_TO_FILE(mInFile, s_dec_ip.pv_stream_buffer, s_dec_ip.u4_num_Bytes);

            mTimeStart = os_get_systime();
            /* Compute time elapsed between end of previous decode()
             * to start of current decode() */
            TIME_DIFF(mTimeEnd, mTimeStart, timeDelay);

            IV_API_CALL_STATUS_T status __attribute((__unused__));
            status = ivdec_api_function(mCodecCtx, (void *)&s_dec_ip, (void *)&s_dec_op);

            bool unsupportedDimensions =
                (IVD_STREAM_WIDTH_HEIGHT_NOT_SUPPORTED == (s_dec_op.u4_error_code & 0xFF));
            bool resChanged = (IVD_RES_CHANGED == (s_dec_op.u4_error_code & 0xFF));
            bool unsupportedLevel = (IH264D_UNSUPPORTED_LEVEL == (s_dec_op.u4_error_code & 0xFF));

            mTimeEnd = os_get_systime();
            /* Compute time taken for decode() */
            TIME_DIFF(mTimeStart, mTimeEnd, timeTaken);

            logv("timeTaken=%6d delay=%6d numBytes=%6d", timeTaken, timeDelay,
                   s_dec_op.u4_num_bytes_consumed);
            if (s_dec_op.u4_frame_decoded_flag && !mFlushNeeded) {
                mFlushNeeded = true;
            }

            if ((inHeader != NULL) && (1 != s_dec_op.u4_frame_decoded_flag)) {
                /* If the input did not contain picture data, then ignore
                 * the associated timestamp */
                mTimeStampsValid[timeStampIx] = false;
            }


            // This is needed to handle CTS DecoderTest testCodecResetsH264WithoutSurface,
            // which is not sending SPS/PPS after port reconfiguration and flush to the codec.
            if (unsupportedDimensions && !mFlushNeeded) {
                bool portWillReset = false;
                mNewWidth = s_dec_op.u4_pic_wd;
                mNewHeight = s_dec_op.u4_pic_ht;

                int32_t err = reInitDecoder(mNewWidth, mNewHeight);
                if (err != 0) {
                    notify(OMX_EventError, OMX_ErrorUnsupportedSetting, err, NULL);
                    mSignalledError = true;
                    return;
                }

                handlePortSettingsChange(&portWillReset, mNewWidth, mNewHeight);

                setDecodeArgs(&s_dec_ip, &s_dec_op, inHeader, outHeader, timeStampIx);

                ivdec_api_function(mCodecCtx, (void *)&s_dec_ip, (void *)&s_dec_op);
                return;
            }

            if (unsupportedLevel && !mFlushNeeded) {

                mNewLevel = 51;

                int32_t err = reInitDecoder(mNewWidth, mNewHeight);
                if (err != 0) {
                    notify(OMX_EventError, OMX_ErrorUnsupportedSetting, err, NULL);
                    mSignalledError = true;
                    return;
                }

                setDecodeArgs(&s_dec_ip, &s_dec_op, inHeader, outHeader, timeStampIx);

                ivdec_api_function(mCodecCtx, (void *)&s_dec_ip, (void *)&s_dec_op);
                return;
            }

            // If the decoder is in the changing resolution mode and there is no output present,
            // that means the switching is done and it's ready to reset the decoder and the plugin.
            if (mChangingResolution && !s_dec_op.u4_output_present) {
                mChangingResolution = false;
                resetDecoder();
                resetPlugin();
                continue;
            }

            if (unsupportedDimensions || resChanged) {
                mChangingResolution = true;
                if (mFlushNeeded) {
                    setFlushMode();
                }

                if (unsupportedDimensions) {
                    mNewWidth = s_dec_op.u4_pic_wd;
                    mNewHeight = s_dec_op.u4_pic_ht;
                    mInitNeeded = true;
                }
                continue;
            }

            if (unsupportedLevel) {

                if (mFlushNeeded) {
                    setFlushMode();
                }

                mNewLevel = 51;
                mInitNeeded = true;
                continue;
            }

            if ((0 < s_dec_op.u4_pic_wd) && (0 < s_dec_op.u4_pic_ht)) {
                uint32_t width = s_dec_op.u4_pic_wd;
                uint32_t height = s_dec_op.u4_pic_ht;
                bool portWillReset = false;
                handlePortSettingsChange(&portWillReset, width, height);

                if (portWillReset) {
                    resetDecoder();
                    return;
                }
            }

            if (s_dec_op.u4_output_present) {
                outHeader->nFilledLen = (outputBufferWidth() * outputBufferHeight() * 3) / 2;

                outHeader->nTimeStamp = mTimeStamps[s_dec_op.u4_ts];
                mTimeStampsValid[s_dec_op.u4_ts] = false;

                outInfo->mOwnedByUs = false;
                outQueue.erase(outQueue.begin());
                outInfo = NULL;
                notifyFillBufferDone(outHeader);
                outHeader = NULL;
            } else {
                /* If in flush mode and no output is returned by the codec,
                 * then come out of flush mode */
                mIsInFlush = false;

                /* If EOS was recieved on input port and there is no output
                 * from the codec, then signal EOS on output port */
                if (mReceivedEOS) {
                    outHeader->nFilledLen = 0;
                    outHeader->nFlags |= OMX_BUFFERFLAG_EOS;

                    outInfo->mOwnedByUs = false;
                    outQueue.erase(outQueue.begin());
                    outInfo = NULL;
                    notifyFillBufferDone(outHeader);
                    outHeader = NULL;
                    resetPlugin();
                }
            }
        }

        if (inHeader != NULL) {
            inInfo->mOwnedByUs = false;
            inQueue.erase(inQueue.begin());
            inInfo = NULL;
            notifyEmptyBufferDone(inHeader);
            inHeader = NULL;
        }
    }
}

void SoftAVCDec::initPorts(
        OMX_U32 numInputBuffers,
        OMX_U32 numOutputBuffers) {
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOMXParams(&def);

    def.nPortIndex = kInputPortIndex;
    def.eDir = OMX_DirInput;
    def.nBufferCountMin = numInputBuffers;
    def.nBufferCountActual = def.nBufferCountMin;
    def.nBufferSize = INPUT_BUF_SIZE;
    def.bEnabled = OMX_TRUE;
    def.bPopulated = OMX_FALSE;
    def.eDomain = OMX_PortDomainVideo;
    def.bBuffersContiguous = OMX_FALSE;
    def.nBufferAlignment = 1;

    //def.format.video.cMIMEType = MEDIA_MIMETYPE_VIDEO_AVC;
    def.format.video.pNativeRender = NULL;
    /* size is initialized in updatePortDefinitions() */
    def.format.video.nBitrate = 0;
    def.format.video.xFramerate = 0;
    def.format.video.bFlagErrorConcealment = OMX_FALSE;
    def.format.video.eCompressionFormat = mCodingType;
    def.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    def.format.video.pNativeWindow = NULL;

    addPort(def);

    def.nPortIndex = kOutputPortIndex;
    def.eDir = OMX_DirOutput;
    def.nBufferCountMin = numOutputBuffers;
    def.nBufferCountActual = def.nBufferCountMin;
    def.bEnabled = OMX_TRUE;
    def.bPopulated = OMX_FALSE;
    def.eDomain = OMX_PortDomainVideo;
    def.bBuffersContiguous = OMX_FALSE;
    def.nBufferAlignment = 2;

    //def.format.video.cMIMEType = const_cast<char *>("video/raw");
    def.format.video.pNativeRender = NULL;
    /* size is initialized in updatePortDefinitions() */
    def.format.video.nBitrate = 0;
    def.format.video.xFramerate = 0;
    def.format.video.bFlagErrorConcealment = OMX_FALSE;
    def.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    def.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    def.format.video.pNativeWindow = NULL;

    addPort(def);

    updatePortDefinitions(true /* updateCrop */, true /* updateInputSize */);
}

void SoftAVCDec::updatePortDefinitions(bool updateCrop, bool updateInputSize) {
    OMX_PARAM_PORTDEFINITIONTYPE *outDef = &editPortInfo(kOutputPortIndex)->mDef;
    outDef->format.video.nFrameWidth = outputBufferWidth();
    outDef->format.video.nFrameHeight = outputBufferHeight();
    outDef->format.video.nStride = outDef->format.video.nFrameWidth;
    outDef->format.video.nSliceHeight = outDef->format.video.nFrameHeight;

    outDef->nBufferSize =
        (outDef->format.video.nStride * outDef->format.video.nSliceHeight * 3) / 2;

    OMX_PARAM_PORTDEFINITIONTYPE *inDef = &editPortInfo(kInputPortIndex)->mDef;
    inDef->format.video.nFrameWidth = mWidth;
    inDef->format.video.nFrameHeight = mHeight;
    // input port is compressed, hence it has no stride
    inDef->format.video.nStride = 0;
    inDef->format.video.nSliceHeight = 0;

    // when output format changes, input buffer size does not actually change
    if (updateInputSize) {
        inDef->nBufferSize = MAX(
                outDef->nBufferSize / mMinCompressionRatio,
                MAX(mMinInputBufferSize, inDef->nBufferSize));
    }

    if (updateCrop) {
        mCropLeft = 0;
        mCropTop = 0;
        mCropWidth = mWidth;
        mCropHeight = mHeight;
    }
}


uint32_t SoftAVCDec::outputBufferWidth() {
    return mIsAdaptive ? mAdaptiveMaxWidth : mWidth;
}

uint32_t SoftAVCDec::outputBufferHeight() {
    return mIsAdaptive ? mAdaptiveMaxHeight : mHeight;
}

void SoftAVCDec::handlePortSettingsChange(
        bool *portWillReset, uint32_t width, uint32_t height,
        CropSettingsMode cropSettingsMode, bool fakeStride) {
    *portWillReset = false;
    bool sizeChanged = (width != mWidth || height != mHeight);
    bool updateCrop = (cropSettingsMode == kCropUnSet);
    bool cropChanged = (cropSettingsMode == kCropChanged);
    bool strideChanged = false;
    if (fakeStride) {
        OMX_PARAM_PORTDEFINITIONTYPE *def = &editPortInfo(kOutputPortIndex)->mDef;
        if (def->format.video.nStride != (OMX_S32)width
                || def->format.video.nSliceHeight != (OMX_U32)height) {
            strideChanged = true;
        }
    }

    if (sizeChanged || cropChanged || strideChanged) {
        mWidth = width;
        mHeight = height;

        if ((sizeChanged && !mIsAdaptive)
            || width > mAdaptiveMaxWidth
            || height > mAdaptiveMaxHeight) {
            if (mIsAdaptive) {
                if (width > mAdaptiveMaxWidth) {
                    mAdaptiveMaxWidth = width;
                }
                if (height > mAdaptiveMaxHeight) {
                    mAdaptiveMaxHeight = height;
                }
            }
            updatePortDefinitions(updateCrop);
            notify(OMX_EventPortSettingsChanged, kOutputPortIndex, 0, NULL);
            mOutputPortSettingsChange = AWAITING_DISABLED;
            *portWillReset = true;
        } else {
            updatePortDefinitions(updateCrop);

            if (fakeStride) {
                // MAJOR HACK that is not pretty, it's just to fool the renderer to read the correct
                // data.
                // Some software decoders (e.g. SoftMPEG4) fill decoded frame directly to output
                // buffer without considering the output buffer stride and slice height. So this is
                // used to signal how the buffer is arranged.  The alternative is to re-arrange the
                // output buffer in SoftMPEG4, but that results in memcopies.
                OMX_PARAM_PORTDEFINITIONTYPE *def = &editPortInfo(kOutputPortIndex)->mDef;
                def->format.video.nStride = mWidth;
                def->format.video.nSliceHeight = mHeight;
            }

            notify(OMX_EventPortSettingsChanged, kOutputPortIndex,
                   OMX_IndexConfigCommonOutputCrop, NULL);
        }
    }
}

OMX_ERRORTYPE SoftAVCDec::internalGetParameter(
        OMX_INDEXTYPE index, OMX_PTR params) {
    switch (index) {
        case OMX_IndexParamVideoPortFormat:
        {
            OMX_VIDEO_PARAM_PORTFORMATTYPE *formatParams =
                (OMX_VIDEO_PARAM_PORTFORMATTYPE *)params;

            if (formatParams->nPortIndex > kMaxPortIndex) {
                return OMX_ErrorBadPortIndex;
            }

            if (formatParams->nIndex != 0) {
                return OMX_ErrorNoMore;
            }

            if (formatParams->nPortIndex == kInputPortIndex) {
                formatParams->eCompressionFormat = mCodingType;
                formatParams->eColorFormat = OMX_COLOR_FormatUnused;
                formatParams->xFramerate = 0;
            } else {
                CHECK_EQ(formatParams->nPortIndex, 1u);

                formatParams->eCompressionFormat = OMX_VIDEO_CodingUnused;
                formatParams->eColorFormat = OMX_COLOR_FormatYUV420Planar;
                formatParams->xFramerate = 0;
            }

            return OMX_ErrorNone;
        }

        case OMX_IndexParamVideoProfileLevelQuerySupported:
        {
            OMX_VIDEO_PARAM_PROFILELEVELTYPE *profileLevel =
                  (OMX_VIDEO_PARAM_PROFILELEVELTYPE *) params;

            if (profileLevel->nPortIndex != kInputPortIndex) {
                loge("Invalid port index: %ld", profileLevel->nPortIndex);
                return OMX_ErrorUnsupportedIndex;
            }

            if (profileLevel->nIndex >= ARRARY_SIZE(kProfileLevels)) {
                return OMX_ErrorNoMore;
            }

            profileLevel->eProfile = kProfileLevels[profileLevel->nIndex].mProfile;
            profileLevel->eLevel   = kProfileLevels[profileLevel->nIndex].mLevel;
            return OMX_ErrorNone;
        }

        default:
            return SimpleSoftOMXComponent::internalGetParameter(index, params);
    }
}

OMX_ERRORTYPE SoftAVCDec::getConfig(
        OMX_INDEXTYPE index, OMX_PTR params) {
    switch (index) {
        case OMX_IndexConfigCommonOutputCrop:
        {
            OMX_CONFIG_RECTTYPE *rectParams = (OMX_CONFIG_RECTTYPE *)params;

            if (rectParams->nPortIndex != kOutputPortIndex) {
                return OMX_ErrorUndefined;
            }

            rectParams->nLeft = mCropLeft;
            rectParams->nTop = mCropTop;
            rectParams->nWidth = mCropWidth;
            rectParams->nHeight = mCropHeight;

            return OMX_ErrorNone;
        }

        default:
            return OMX_ErrorUnsupportedIndex;
    }
}

}  // namespace omxil
