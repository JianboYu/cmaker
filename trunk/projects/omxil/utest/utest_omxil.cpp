#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG "utest_omxil"

#include <string.h>
#include <os_typedefs.h>
#include <os_assert.h>
#include <os_log.h>
#include <os_time.h>
#include <os_thread.h>
#include <utility_circle_queue.h>
#include <core_scoped_ptr.h>
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Index.h>
#include <OMX_Video.h>
#include <OMX_AudioExt.h>
#include <OMX_IndexExt.h>

using namespace os;
using namespace core;

template<class T>
static void InitOMXParams(T *params) {
  memset(params, 0, sizeof(T));
  params->nSize = sizeof(T);
  params->nVersion.s.nVersionMajor = 1;
  params->nVersion.s.nVersionMinor = 0;
  params->nVersion.s.nRevision = 0;
  params->nVersion.s.nStep = 0;
}

typedef struct OMXContext {
  OMX_HANDLETYPE hComponent;
  OMX_BUFFERHEADERTYPE *inBuffer[8];
  OMX_BUFFERHEADERTYPE *outBuffer[8];
  cirq_handle fbd;
  cirq_handle ebd;
  os::Thread *thread;
  OMX_TICKS  ts;
  bool encode;
  bool video;
  FILE *fp;
  FILE *fp_encoded;
  FILE *fp_len;
}OMXContext;

OMX_ERRORTYPE sEventHandler(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_IN OMX_PTR pAppData,
  OMX_IN OMX_EVENTTYPE eEvent,
  OMX_IN OMX_U32 nData1,
  OMX_IN OMX_U32 nData2,
  OMX_IN OMX_PTR pEventData) {
    logv("EventHandler com: %p appdata: %p\n", hComponent, pAppData);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE sEmptyBufferDone(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_IN OMX_PTR pAppData,
  OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) {
    logv("EmptyBufferDone com: %p appdata: %p\n", hComponent, pAppData);
    OMXContext *omx_ctx = (OMXContext *)pAppData;
    int32_t status = cirq_enqueue(omx_ctx->ebd, pBuffer);
    CHECK_EQ(0, status);

    return OMX_ErrorNone;
}
OMX_ERRORTYPE sFillBufferDone(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_IN OMX_PTR pAppData,
  OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) {
    logv("FillBufferDone com: %p appdata: %p\n", hComponent, pAppData);
    OMXContext *omx_ctx = (OMXContext *)pAppData;
    int32_t status = cirq_enqueue(omx_ctx->fbd, pBuffer);
    CHECK_EQ(0, status);
    return OMX_ErrorNone;
}

bool encoder_loop(void *ctx) {
  OMX_ERRORTYPE oRet = OMX_ErrorNone;
  OMXContext *omx_ctx = (OMXContext *)ctx;

  OMX_HANDLETYPE hComponent = omx_ctx->hComponent;
  void *pdata = NULL;
  cirq_dequeue(omx_ctx->ebd, &pdata);
  OMX_BUFFERHEADERTYPE* pBuffer = (OMX_BUFFERHEADERTYPE*)pdata;
  if (pBuffer) {
    pBuffer->nFlags = 0;
    pBuffer->nFilledLen = 1280*720*3 >> 1;
    pBuffer->nOffset = 0;
    pBuffer->nTimeStamp = omx_ctx->ts;

    uint32_t readed = fread(pBuffer->pBuffer, 1, pBuffer->nFilledLen, omx_ctx->fp);
    if (readed < pBuffer->nFilledLen) {
      rewind(omx_ctx->fp);
      readed = fread(pBuffer->pBuffer, 1, pBuffer->nFilledLen, omx_ctx->fp);
      CHECK_EQ(readed, pBuffer->nFilledLen);
    }
    oRet = OMX_EmptyThisBuffer(hComponent, pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
    logv("EmptyThisBuffer: %p\n", pBuffer);
    omx_ctx->ts += 3600;
  }

  pdata = NULL;
  cirq_dequeue(omx_ctx->fbd, &pdata);
  pBuffer = (OMX_BUFFERHEADERTYPE*)pdata;
  if (pBuffer ) {
    if (pBuffer->nFilledLen > 0) {
      uint32_t writed = fwrite(pBuffer->pBuffer, 1, pBuffer->nFilledLen, omx_ctx->fp_encoded);
      CHECK_EQ(writed, pBuffer->nFilledLen);
      writed = fprintf(omx_ctx->fp_len, "%d\n", (int32_t)pBuffer->nFilledLen);
      CHECK_GT(writed, 0);
      fflush(omx_ctx->fp_encoded);
      fflush(omx_ctx->fp_len);
    }
    pBuffer->nFlags = 0;
    pBuffer->nFilledLen = 0;
    pBuffer->nOffset = 0;
    pBuffer->nTimeStamp = 0;
    oRet = OMX_FillThisBuffer(hComponent, pBuffer);
    logv("FillThisBuffer: %p\n", pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }
  os_msleep(40);
  return true;
}

bool decoder_loop(void *ctx) {
  OMX_ERRORTYPE oRet = OMX_ErrorNone;
  OMXContext *omx_ctx = (OMXContext *)ctx;

  OMX_HANDLETYPE hComponent = omx_ctx->hComponent;
  void *pdata = NULL;
  cirq_dequeue(omx_ctx->ebd, &pdata);
  OMX_BUFFERHEADERTYPE* pBuffer = (OMX_BUFFERHEADERTYPE*)pdata;
  if (pBuffer) {
    int32_t len = 0;
    int32_t readed = fscanf(omx_ctx->fp_len, "%d\n", &len);
    if (readed < 0 || len <= 0) {
      rewind(omx_ctx->fp_encoded);
      rewind(omx_ctx->fp_len);
      readed = fscanf(omx_ctx->fp_len, "%d\n", &len);
      if (readed < 0 || len <= 0) {
        CHECK(!"BUG");
      }
    }
    readed = fread(pBuffer->pBuffer, 1, len, omx_ctx->fp_encoded);
    CHECK_EQ(readed, len);

    pBuffer->nFlags = 0;
    pBuffer->nFilledLen = len;
    pBuffer->nOffset = 0;
    pBuffer->nTimeStamp = omx_ctx->ts;
    oRet = OMX_EmptyThisBuffer(hComponent, pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
    logv("EmptyThisBuffer: %p\n", pBuffer);
    omx_ctx->ts += 3600;
  }

  pdata = NULL;
  cirq_dequeue(omx_ctx->fbd, &pdata);
  pBuffer = (OMX_BUFFERHEADERTYPE*)pdata;
  if (pBuffer) {
    if (pBuffer->nFilledLen > 0) {
      uint32_t writed = fwrite(pBuffer->pBuffer, 1, pBuffer->nFilledLen, omx_ctx->fp);
      CHECK_EQ(writed, pBuffer->nFilledLen);
      fflush(omx_ctx->fp);
    }
    pBuffer->nFlags = 0;
    pBuffer->nFilledLen = 0;
    pBuffer->nOffset = 0;
    pBuffer->nTimeStamp = 0;
    oRet = OMX_FillThisBuffer(hComponent, pBuffer);
    logv("FillThisBuffer: %p\n", pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }
  os_msleep(40);
  return true;
}
bool thread_loop(void *ctx) {
  OMXContext *omx_ctx = (OMXContext *)ctx;
  if (omx_ctx->encode)
    return encoder_loop(ctx);
  else
    return decoder_loop(ctx);
}

void DumpPortDefine(OMX_PARAM_PORTDEFINITIONTYPE *def) {
    logv("-----------------Port Define---------------\n");
    logv("Index: %d\n", def->nPortIndex);
    logv("Dir: %s\n", def->eDir == OMX_DirInput ? "DirInput" : "DirOuput");
    logv("BufMinCount: %d\n", def->nBufferCountMin);
    logv("BufActCount: %d\n", def->nBufferCountActual);
    logv("Domain: %s\n", def->eDomain == OMX_PortDomainVideo ? "Video" : "Audio");
    if (def->eDomain == OMX_PortDomainVideo) {
      logv("Width: %d\n", def->format.video.nFrameWidth);
      logv("Height: %d\n", def->format.video.nFrameHeight);
      logv("Stride: %d\n", def->format.video.nStride);
      logv("SliceH: %d\n", def->format.video.nSliceHeight);
      logv("Bitrate: %d\n", def->format.video.nBitrate);
      logv("Framerate: %d\n", def->format.video.xFramerate >> 16);
      logv("BufAlign: %d\n", def->nBufferAlignment);
      logv("CompressFmt: %x\n", def->format.video.eCompressionFormat);
      logv("ColorFmt: %x\n", def->format.video.eColorFormat);
    }
}
int32_t ve_main(int argc, char *argv[]) {
  OMX_ERRORTYPE oRet = OMX_ErrorNone;
  oRet = OMX_Init();
  CHECK_EQ(oRet, OMX_ErrorNone);
  scoped_array<char> comp_name(new char [128]);
  for (int32_t i = 0; ; ++i) {
    oRet = OMX_ComponentNameEnum((OMX_STRING)comp_name.get(), 128, i);
    if (OMX_ErrorNoMore == oRet)
      break;
    CHECK_EQ(oRet, OMX_ErrorNone);

    log_verbose("tag", "Component[%d]: %s \n", i + 1, comp_name.get());
  }
  OMXContext *omx_ctx = (OMXContext*)malloc(sizeof(OMXContext));
  CHECK(omx_ctx);
  int32_t status = -1;
  status = cirq_create(&omx_ctx->fbd, 2);
  CHECK_EQ(0, status);
  status = cirq_create(&omx_ctx->ebd, 2);
  CHECK_EQ(0, status);
  omx_ctx->thread = os::Thread::Create(thread_loop, omx_ctx);
  omx_ctx->ts = 3600;
  omx_ctx->encode = true;
  omx_ctx->fp = fopen("i420_1280x720_8pi_KristenAndSara_60.yuv", "r");
  omx_ctx->fp_encoded = fopen("i420_1280x720_8pi_KristenAndSara_60.h264", "w+");
  omx_ctx->fp_len = fopen("i420_1280x720_8pi_KristenAndSara_60.len", "w+");

  OMX_CALLBACKTYPE omx_cb;
  omx_cb.EventHandler = sEventHandler;
  omx_cb.FillBufferDone = sFillBufferDone;
  omx_cb.EmptyBufferDone = sEmptyBufferDone;
  OMX_HANDLETYPE pHandle;
  oRet = OMX_GetHandle(&pHandle,
    (OMX_STRING)"OMX.omxil.h264.encoder",
    (OMX_PTR)omx_ctx,
    &omx_cb);
  CHECK_EQ(oRet, OMX_ErrorNone);
  omx_ctx->hComponent = pHandle;

  OMX_PARAM_PORTDEFINITIONTYPE def;
  InitOMXParams(&def);
  def.nPortIndex = 0;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);

  def.format.video.nFrameWidth = 1280;
  def.format.video.nFrameHeight = 720;
  def.format.video.nStride = def.format.video.nFrameWidth;
  def.format.video.nSliceHeight = def.format.video.nFrameHeight;
  def.format.video.nBitrate = 0;
  def.format.video.xFramerate = 30 << 16;
  def.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
  def.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
  DumpPortDefine(&def);
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);

  def.nPortIndex = 1;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);
  def.format.video.nFrameWidth = 1280;
  def.format.video.nFrameHeight = 720;
  def.format.video.nStride = def.format.video.nFrameWidth;
  def.format.video.nSliceHeight = def.format.video.nFrameHeight;
  def.format.video.nBitrate = 1000;
  def.format.video.xFramerate = 0;
  def.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  def.format.video.eColorFormat = OMX_COLOR_FormatUnused;
  DumpPortDefine(&def);
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < 2; ++i) {
    oRet = OMX_AllocateBuffer(pHandle, &omx_ctx->inBuffer[i], 0, NULL, 1280*720*3 >> 1);
    CHECK_EQ(oRet, OMX_ErrorNone);
    CHECK(omx_ctx->inBuffer[i]);
    logv("Port: %d index: %d Allocate Buffer: %p len: %d\n",
        0, i, omx_ctx->inBuffer[i]->pBuffer, omx_ctx->inBuffer[i]->nFilledLen);
  }

  for (int32_t i = 0; i < 2; ++i) {
    oRet = OMX_AllocateBuffer(pHandle, &omx_ctx->outBuffer[i], 1, NULL, 1280*720*3 >> 1);
    CHECK_EQ(oRet, OMX_ErrorNone);
    CHECK(omx_ctx->outBuffer[i]);
    logv("Port: %d index: %d Allocate Buffer: %p len: %d\n",
        1, i, omx_ctx->outBuffer[i]->pBuffer, omx_ctx->outBuffer[i]->nFilledLen);
  }
  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

#if 0
  OMX_BUFFERHEADERTYPE* pBuffer = NULL;
  for (int32_t i = 0; i < 2; ++i) {
    pBuffer = omx_ctx->inBuffer[i];
    pBuffer->nFlags = 0;
    pBuffer->nFilledLen = 1280*720*3 >> 1;
    pBuffer->nOffset = 0;
    pBuffer->nTimeStamp = omx_ctx->ts;
    oRet = OMX_EmptyThisBuffer(pHandle, pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
    omx_ctx->ts += 3600;
  }

  for (int32_t i = 0; i < 2; ++i) {
    pBuffer = omx_ctx->outBuffer[i];
    pBuffer->nFlags = 0;
    pBuffer->nFilledLen = 0;
    pBuffer->nOffset = 0;
    pBuffer->nTimeStamp = 0;
    oRet = OMX_FillThisBuffer(pHandle, pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }
#else
  for (int32_t i = 0; i < 2; ++i) {
    int32_t status = cirq_enqueue(omx_ctx->ebd, omx_ctx->inBuffer[i]);
    CHECK_EQ(0, status);
    status = cirq_enqueue(omx_ctx->fbd, omx_ctx->outBuffer[i]);
    CHECK_EQ(0, status);
  }
#endif

  uint32_t tid = 0;
  omx_ctx->thread->start(tid);
  os_sleep(1000);

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < 2; ++i) {
    oRet = OMX_FreeBuffer(pHandle, 0, omx_ctx->inBuffer[i]);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }
  for (int32_t i = 0; i < 2; ++i) {
    oRet = OMX_FreeBuffer(pHandle, 1, omx_ctx->outBuffer[i]);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);


  oRet = OMX_FreeHandle(pHandle);
  CHECK_EQ(oRet, OMX_ErrorNone);

  oRet = OMX_Deinit();
  CHECK_EQ(oRet, OMX_ErrorNone);

  omx_ctx->thread->stop();
  delete omx_ctx->thread;
  status = cirq_destory(omx_ctx->fbd);
  CHECK_EQ(0, status);
  status = cirq_destory(omx_ctx->ebd);
  CHECK_EQ(0, status);
  fclose(omx_ctx->fp);
  fclose(omx_ctx->fp_encoded);

  free(omx_ctx);
  return 0;
}

int32_t vd_main(int argc, char *argv[]) {
  OMX_ERRORTYPE oRet = OMX_ErrorNone;
  oRet = OMX_Init();
  CHECK_EQ(oRet, OMX_ErrorNone);
  scoped_array<char> comp_name(new char [128]);
  for (int32_t i = 0; ; ++i) {
    oRet = OMX_ComponentNameEnum((OMX_STRING)comp_name.get(), 128, i);
    if (OMX_ErrorNoMore == oRet)
      break;
    CHECK_EQ(oRet, OMX_ErrorNone);

    log_verbose("tag", "Component[%d]: %s \n", i + 1, comp_name.get());
  }
  OMXContext *omx_ctx = (OMXContext*)malloc(sizeof(OMXContext));
  CHECK(omx_ctx);
  int32_t status = -1;
  status = cirq_create(&omx_ctx->fbd, 8);
  CHECK_EQ(0, status);
  status = cirq_create(&omx_ctx->ebd, 8);
  CHECK_EQ(0, status);
  omx_ctx->thread = os::Thread::Create(thread_loop, omx_ctx);
  omx_ctx->ts = 3600;
  omx_ctx->encode = false;
  omx_ctx->fp = fopen("output-i420_1280x720_8pi_KristenAndSara_60.yuv", "w+");
  omx_ctx->fp_encoded = fopen("i420_1280x720_8pi_KristenAndSara_60.h264", "r");
  omx_ctx->fp_len = fopen("i420_1280x720_8pi_KristenAndSara_60.len", "r");

  OMX_CALLBACKTYPE omx_cb;
  omx_cb.EventHandler = sEventHandler;
  omx_cb.FillBufferDone = sFillBufferDone;
  omx_cb.EmptyBufferDone = sEmptyBufferDone;
  OMX_HANDLETYPE pHandle;
  oRet = OMX_GetHandle(&pHandle,
    (OMX_STRING)"OMX.omxil.h264.decoder",
    (OMX_PTR)omx_ctx,
    &omx_cb);
  CHECK_EQ(oRet, OMX_ErrorNone);
  omx_ctx->hComponent = pHandle;

  OMX_PARAM_PORTDEFINITIONTYPE def;
  InitOMXParams(&def);
  def.nPortIndex = 0;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);
  def.format.video.nFrameWidth = 1280;
  def.format.video.nFrameHeight = 720;
  def.format.video.nStride = def.format.video.nFrameWidth;
  def.format.video.nSliceHeight = def.format.video.nFrameHeight;
  def.format.video.nBitrate = 1000;
  def.format.video.xFramerate = 0;
  def.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  def.format.video.eColorFormat = OMX_COLOR_FormatUnused;
  DumpPortDefine(&def);
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);

  def.nPortIndex = 1;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);

  def.format.video.nFrameWidth = 1280;
  def.format.video.nFrameHeight = 720;
  def.format.video.nStride = def.format.video.nFrameWidth;
  def.format.video.nSliceHeight = def.format.video.nFrameHeight;
  def.format.video.nBitrate = 0;
  def.format.video.xFramerate = 30 << 16;
  def.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
  def.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
  DumpPortDefine(&def);
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);


  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < 8; ++i) {
    oRet = OMX_AllocateBuffer(pHandle, &omx_ctx->inBuffer[i], 0, NULL, 1280*720*3 >> 1);
    CHECK_EQ(oRet, OMX_ErrorNone);
    CHECK(omx_ctx->inBuffer[i]);
    logv("Port: %d index: %d Allocate Buffer: %p len: %d\n",
        0, i, omx_ctx->inBuffer[i]->pBuffer, omx_ctx->inBuffer[i]->nFilledLen);
  }

  for (int32_t i = 0; i < 8; ++i) {
    oRet = OMX_AllocateBuffer(pHandle, &omx_ctx->outBuffer[i], 1, NULL, 1280*720*3 >> 1);
    CHECK_EQ(oRet, OMX_ErrorNone);
    CHECK(omx_ctx->outBuffer[i]);
    logv("Port: %d index: %d Allocate Buffer: %p len: %d\n",
        1, i, omx_ctx->outBuffer[i]->pBuffer, omx_ctx->outBuffer[i]->nFilledLen);
  }
  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < 8; ++i) {
    int32_t status = cirq_enqueue(omx_ctx->ebd, omx_ctx->inBuffer[i]);
    CHECK_EQ(0, status);
    status = cirq_enqueue(omx_ctx->fbd, omx_ctx->outBuffer[i]);
    CHECK_EQ(0, status);
  }

  uint32_t tid = 0;
  omx_ctx->thread->start(tid);
  os_sleep(1000);

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < 2; ++i) {
    oRet = OMX_FreeBuffer(pHandle, 0, omx_ctx->inBuffer[i]);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }
  for (int32_t i = 0; i < 2; ++i) {
    oRet = OMX_FreeBuffer(pHandle, 1, omx_ctx->outBuffer[i]);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);


  oRet = OMX_FreeHandle(pHandle);
  CHECK_EQ(oRet, OMX_ErrorNone);

  oRet = OMX_Deinit();
  CHECK_EQ(oRet, OMX_ErrorNone);

  omx_ctx->thread->stop();
  delete omx_ctx->thread;
  status = cirq_destory(omx_ctx->fbd);
  CHECK_EQ(0, status);
  status = cirq_destory(omx_ctx->ebd);
  CHECK_EQ(0, status);
  fclose(omx_ctx->fp);
  fclose(omx_ctx->fp_encoded);
  fclose(omx_ctx->fp_len);

  free(omx_ctx);
  return 0;
}

bool audio_encoder_loop(void *ctx) {
  OMX_ERRORTYPE oRet = OMX_ErrorNone;
  OMXContext *omx_ctx = (OMXContext *)ctx;

  OMX_HANDLETYPE hComponent = omx_ctx->hComponent;
  void *pdata = NULL;
  cirq_dequeue(omx_ctx->ebd, &pdata);
  OMX_BUFFERHEADERTYPE* pBuffer = (OMX_BUFFERHEADERTYPE*)pdata;
  if (pBuffer) {
    pBuffer->nFlags = 0;
    pBuffer->nFilledLen = 1024 * sizeof(int16_t) * 2;
    pBuffer->nOffset = 0;
    pBuffer->nTimeStamp = omx_ctx->ts;

    uint32_t readed = fread(pBuffer->pBuffer, 1, pBuffer->nFilledLen, omx_ctx->fp);
    if (readed < pBuffer->nFilledLen) {
      rewind(omx_ctx->fp);
      readed = fread(pBuffer->pBuffer, 1, pBuffer->nFilledLen, omx_ctx->fp);
      CHECK_EQ(readed, pBuffer->nFilledLen);
    }

    oRet = OMX_EmptyThisBuffer(hComponent, pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
    logv("EmptyThisBuffer: %p\n", pBuffer);
    omx_ctx->ts += 3600;
  }

  pdata = NULL;
  cirq_dequeue(omx_ctx->fbd, &pdata);
  pBuffer = (OMX_BUFFERHEADERTYPE*)pdata;
  if (pBuffer ) {
    if (pBuffer->nFilledLen > 0) {
      uint32_t writed = fwrite(pBuffer->pBuffer, 1, pBuffer->nFilledLen, omx_ctx->fp_encoded);
      CHECK_EQ(writed, pBuffer->nFilledLen);
      writed = fprintf(omx_ctx->fp_len, "%d\n", (int32_t)pBuffer->nFilledLen);
      CHECK_GT(writed, 0);
      fflush(omx_ctx->fp_encoded);
      fflush(omx_ctx->fp_len);
    }
    pBuffer->nFlags = 0;
    pBuffer->nFilledLen = 0;
    pBuffer->nOffset = 0;
    pBuffer->nTimeStamp = 0;
    oRet = OMX_FillThisBuffer(hComponent, pBuffer);
    logv("FillThisBuffer: %p\n", pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }
  os_msleep(40);
  return true;
}

bool audio_decoder_loop(void *ctx) {
  OMX_ERRORTYPE oRet = OMX_ErrorNone;
  OMXContext *omx_ctx = (OMXContext *)ctx;

  OMX_HANDLETYPE hComponent = omx_ctx->hComponent;
  void *pdata = NULL;
  cirq_dequeue(omx_ctx->ebd, &pdata);
  OMX_BUFFERHEADERTYPE* pBuffer = (OMX_BUFFERHEADERTYPE*)pdata;
  if (pBuffer) {
    int32_t len = 0;
    int32_t readed = fscanf(omx_ctx->fp_len, "%d\n", &len);
    if (readed < 0 || len <= 0) {
      rewind(omx_ctx->fp_encoded);
      rewind(omx_ctx->fp_len);
      readed = fscanf(omx_ctx->fp_len, "%d\n", &len);
      if (readed < 0 || len <= 0) {
        CHECK(!"BUG");
      }
    }
    readed = fread(pBuffer->pBuffer, 1, len, omx_ctx->fp_encoded);
    CHECK_EQ(readed, len);

    pBuffer->nFlags = 0;
    pBuffer->nFilledLen = len;
    pBuffer->nOffset = 0;
    pBuffer->nTimeStamp = omx_ctx->ts;
    oRet = OMX_EmptyThisBuffer(hComponent, pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
    logv("EmptyThisBuffer: %p\n", pBuffer);
    omx_ctx->ts += 3600;
  }

  pdata = NULL;
  cirq_dequeue(omx_ctx->fbd, &pdata);
  pBuffer = (OMX_BUFFERHEADERTYPE*)pdata;
  if (pBuffer) {
    if (pBuffer->nFilledLen > 0) {
      uint32_t writed = fwrite(pBuffer->pBuffer, 1, pBuffer->nFilledLen, omx_ctx->fp);
      CHECK_EQ(writed, pBuffer->nFilledLen);
      fflush(omx_ctx->fp);
    }
    pBuffer->nFlags = 0;
    pBuffer->nFilledLen = 0;
    pBuffer->nOffset = 0;
    pBuffer->nTimeStamp = 0;
    oRet = OMX_FillThisBuffer(hComponent, pBuffer);
    logv("FillThisBuffer: %p\n", pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }
  os_msleep(40);
  return true;
}


bool audio_thread_loop(void *ctx) {
  OMXContext *omx_ctx = (OMXContext *)ctx;
  if (omx_ctx->encode)
    return audio_encoder_loop(ctx);
  else
    return audio_decoder_loop(ctx);
}

int32_t ae_main(int argc, char *argv[]) {
  OMX_ERRORTYPE oRet = OMX_ErrorNone;
  oRet = OMX_Init();
  CHECK_EQ(oRet, OMX_ErrorNone);
  scoped_array<char> comp_name(new char [128]);
  for (int32_t i = 0; ; ++i) {
    oRet = OMX_ComponentNameEnum((OMX_STRING)comp_name.get(), 128, i);
    if (OMX_ErrorNoMore == oRet)
      break;
    CHECK_EQ(oRet, OMX_ErrorNone);

    log_verbose("tag", "Component[%d]: %s \n", i + 1, comp_name.get());
  }
  OMXContext *omx_ctx = (OMXContext*)malloc(sizeof(OMXContext));
  CHECK(omx_ctx);
  int32_t status = -1;
  status = cirq_create(&omx_ctx->fbd, 4);
  CHECK_EQ(0, status);
  status = cirq_create(&omx_ctx->ebd, 4);
  CHECK_EQ(0, status);
  omx_ctx->thread = os::Thread::Create(audio_thread_loop, omx_ctx);
  omx_ctx->ts = 3600;
  omx_ctx->encode = true;
  omx_ctx->fp = fopen("32k-mono-16width.pcm", "r");
  omx_ctx->fp_encoded = fopen("32k-mono-16width.aac", "w+");
  omx_ctx->fp_len = fopen("32k-mono-16width-aac.len", "w+");

  OMX_U32 numChannels = 1;
  OMX_U32 sampleRate = 32000;
  OMX_U32 bitRate = 8000;

  OMX_CALLBACKTYPE omx_cb;
  omx_cb.EventHandler = sEventHandler;
  omx_cb.FillBufferDone = sFillBufferDone;
  omx_cb.EmptyBufferDone = sEmptyBufferDone;
  OMX_HANDLETYPE pHandle;
  oRet = OMX_GetHandle(&pHandle,
    (OMX_STRING)"OMX.omxil.aac.encoder",
    (OMX_PTR)omx_ctx,
    &omx_cb);
  CHECK_EQ(oRet, OMX_ErrorNone);
  omx_ctx->hComponent = pHandle;

  OMX_PARAM_PORTDEFINITIONTYPE def;
  InitOMXParams(&def);
  def.nPortIndex = 0;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);
  def.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  DumpPortDefine(&def);
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);

  def.nPortIndex = 1;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);
  def.format.audio.bFlagErrorConcealment = OMX_TRUE;
  def.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
  DumpPortDefine(&def);
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);

  OMX_AUDIO_PARAM_PCMMODETYPE pcmParams;
  InitOMXParams(&pcmParams);
  pcmParams.nPortIndex = 0;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamAudioPcm, &pcmParams);
  CHECK_EQ(oRet, OMX_ErrorNone);
  pcmParams.nChannels = numChannels;
  pcmParams.eNumData = OMX_NumericalDataSigned;
  pcmParams.bInterleaved = OMX_TRUE;
  pcmParams.nBitPerSample = 16;
  pcmParams.nSamplingRate = sampleRate;
  pcmParams.ePCMMode = OMX_AUDIO_PCMModeLinear;
  pcmParams.eChannelMapping[0]=OMX_AUDIO_ChannelCF;
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamAudioPcm, &pcmParams);
  CHECK_EQ(oRet, OMX_ErrorNone);

  OMX_AUDIO_PARAM_AACPROFILETYPE profile;
  //InitOMXParams(&profile);
  //profile.nPortIndex = 0;
  //oRet = OMX_GetParameter(pHandle, OMX_IndexParamAudioAac, &profile);
  //profile.nChannels = numChannels;
  //profile.nSampleRate = sampleRate;
  //profile.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4FF;
  //oRet = OMX_SetParameter(pHandle, OMX_IndexParamAudioAac, &profile);
  //CHECK_EQ(oRet, OMX_ErrorNone);

  //OMX_AUDIO_PARAM_ANDROID_AACPRESENTATIONTYPE presentation;
  //presentation.nMaxOutputChannels = 6;
  //presentation.nDrcCut = -1;
  //presentation.nDrcBoost = -1;
  //presentation.nHeavyCompression = -1;
  //presentation.nTargetReferenceLevel = -1;
  //presentation.nEncodedTargetLevel = -1;
  //presentation.nPCMLimiterEnable = -1;
  //oRet = OMX_SetParameter(pHandle, (OMX_INDEXTYPE)OMX_IndexParamAudioAndroidAacPresentation, &presentation);
  //CHECK_EQ(oRet, OMX_ErrorNone);

  //OMX_AUDIO_PARAM_AACPROFILETYPE profile;

  InitOMXParams(&profile);
  profile.nPortIndex = 1;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamAudioAac, &profile);
  CHECK_EQ(oRet, OMX_ErrorNone);
  profile.nChannels = numChannels;
  profile.eChannelMode = OMX_AUDIO_ChannelModeMono;//OMX_AUDIO_ChannelModeStereo
  profile.nSampleRate = sampleRate;
  profile.nBitRate = bitRate;
  profile.nAudioBandWidth = 0;
  profile.nFrameLength = 0;
  profile.nAACtools = OMX_AUDIO_AACToolAll;
  profile.nAACERtools = OMX_AUDIO_AACERNone;
  profile.eAACProfile = (OMX_AUDIO_AACPROFILETYPE)OMX_AUDIO_AACObjectNull;
  profile.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4FF;
  profile.nAACtools |= OMX_AUDIO_AACToolAndroidSSBR | OMX_AUDIO_AACToolAndroidDSBR;
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamAudioAac, &profile);
  CHECK_EQ(oRet, OMX_ErrorNone);

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < 4; ++i) {
    oRet = OMX_AllocateBuffer(pHandle, &omx_ctx->inBuffer[i], 0, NULL, 1024 * sizeof(int16_t) * 2);
    CHECK_EQ(oRet, OMX_ErrorNone);
    CHECK(omx_ctx->inBuffer[i]);
    logv("Port: %d index: %d Allocate Buffer: %p len: %d\n",
        0, i, omx_ctx->inBuffer[i]->pBuffer, omx_ctx->inBuffer[i]->nFilledLen);
  }

  for (int32_t i = 0; i < 4; ++i) {
    oRet = OMX_AllocateBuffer(pHandle, &omx_ctx->outBuffer[i], 1, NULL, 8192);
    CHECK_EQ(oRet, OMX_ErrorNone);
    CHECK(omx_ctx->outBuffer[i]);
    logv("Port: %d index: %d Allocate Buffer: %p len: %d\n",
        1, i, omx_ctx->outBuffer[i]->pBuffer, omx_ctx->outBuffer[i]->nFilledLen);
  }
  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < 4; ++i) {
    int32_t status = cirq_enqueue(omx_ctx->ebd, omx_ctx->inBuffer[i]);
    CHECK_EQ(0, status);
    status = cirq_enqueue(omx_ctx->fbd, omx_ctx->outBuffer[i]);
    CHECK_EQ(0, status);
  }

  uint32_t tid = 0;
  omx_ctx->thread->start(tid);
  os_sleep(1000);

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < 4; ++i) {
    oRet = OMX_FreeBuffer(pHandle, 0, omx_ctx->inBuffer[i]);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }
  for (int32_t i = 0; i < 4; ++i) {
    oRet = OMX_FreeBuffer(pHandle, 1, omx_ctx->outBuffer[i]);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);


  oRet = OMX_FreeHandle(pHandle);
  CHECK_EQ(oRet, OMX_ErrorNone);

  oRet = OMX_Deinit();
  CHECK_EQ(oRet, OMX_ErrorNone);

  omx_ctx->thread->stop();
  delete omx_ctx->thread;
  status = cirq_destory(omx_ctx->fbd);
  CHECK_EQ(0, status);
  status = cirq_destory(omx_ctx->ebd);
  CHECK_EQ(0, status);
  fclose(omx_ctx->fp);
  fclose(omx_ctx->fp_encoded);
  fclose(omx_ctx->fp_len);

  free(omx_ctx);
  return 0;
}

int32_t ad_main(int argc, char *argv[]) {
  OMX_ERRORTYPE oRet = OMX_ErrorNone;
  oRet = OMX_Init();
  CHECK_EQ(oRet, OMX_ErrorNone);
  scoped_array<char> comp_name(new char [128]);
  for (int32_t i = 0; ; ++i) {
    oRet = OMX_ComponentNameEnum((OMX_STRING)comp_name.get(), 128, i);
    if (OMX_ErrorNoMore == oRet)
      break;
    CHECK_EQ(oRet, OMX_ErrorNone);

    log_verbose("tag", "Component[%d]: %s \n", i + 1, comp_name.get());
  }
  OMXContext *omx_ctx = (OMXContext*)malloc(sizeof(OMXContext));
  CHECK(omx_ctx);
  int32_t status = -1;
  status = cirq_create(&omx_ctx->fbd, 4);
  CHECK_EQ(0, status);
  status = cirq_create(&omx_ctx->ebd, 4);
  CHECK_EQ(0, status);
  omx_ctx->thread = os::Thread::Create(audio_thread_loop, omx_ctx);
  omx_ctx->ts = 3600;
  omx_ctx->encode = false;
  omx_ctx->fp = fopen("output-32k-mono-16width.pcm", "w+");
  omx_ctx->fp_encoded = fopen("32k-mono-16width.aac", "r");
  omx_ctx->fp_len = fopen("32k-mono-16width-aac.len", "r");

  OMX_U32 numChannels = 1;
  OMX_U32 sampleRate = 32000;
  OMX_U32 bitRate = 8000;

  OMX_CALLBACKTYPE omx_cb;
  omx_cb.EventHandler = sEventHandler;
  omx_cb.FillBufferDone = sFillBufferDone;
  omx_cb.EmptyBufferDone = sEmptyBufferDone;
  OMX_HANDLETYPE pHandle;
  oRet = OMX_GetHandle(&pHandle,
    (OMX_STRING)"OMX.omxil.aac.decoder",
    (OMX_PTR)omx_ctx,
    &omx_cb);
  CHECK_EQ(oRet, OMX_ErrorNone);
  omx_ctx->hComponent = pHandle;

  OMX_PARAM_PORTDEFINITIONTYPE def;
  InitOMXParams(&def);
  def.nPortIndex = 0;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);
  DumpPortDefine(&def);
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);

  OMX_AUDIO_PARAM_AACPROFILETYPE profile;
  InitOMXParams(&profile);
  profile.nPortIndex = 0;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamAudioAac, &profile);
  profile.nChannels = numChannels;
  profile.nSampleRate = sampleRate;
  profile.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4FF;
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamAudioAac, &profile);
  CHECK_EQ(oRet, OMX_ErrorNone);

  OMX_AUDIO_PARAM_ANDROID_AACPRESENTATIONTYPE presentation;
  presentation.nMaxOutputChannels = 6;
  presentation.nDrcCut = -1;
  presentation.nDrcBoost = -1;
  presentation.nHeavyCompression = -1;
  presentation.nTargetReferenceLevel = -1;
  presentation.nEncodedTargetLevel = -1;
  presentation.nPCMLimiterEnable = -1;
  oRet = OMX_SetParameter(pHandle, (OMX_INDEXTYPE)OMX_IndexParamAudioAndroidAacPresentation, &presentation);
  CHECK_EQ(oRet, OMX_ErrorNone);

  //OMX_AUDIO_PARAM_AACPROFILETYPE profile;
  InitOMXParams(&profile);
  profile.nPortIndex = 0;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamAudioAac, &profile);
  CHECK_EQ(oRet, OMX_ErrorNone);
  profile.nChannels = numChannels;
  profile.eChannelMode = OMX_AUDIO_ChannelModeMono;//OMX_AUDIO_ChannelModeStereo
  profile.nSampleRate = sampleRate;
  profile.nBitRate = bitRate;
  profile.nAudioBandWidth = 0;
  profile.nFrameLength = 0;
  profile.nAACtools = OMX_AUDIO_AACToolAll;
  profile.nAACERtools = OMX_AUDIO_AACERNone;
  profile.eAACProfile = (OMX_AUDIO_AACPROFILETYPE)OMX_AUDIO_AACObjectNull;
  profile.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4FF;
  profile.nAACtools |= OMX_AUDIO_AACToolAndroidSSBR | OMX_AUDIO_AACToolAndroidDSBR;
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamAudioAac, &profile);
  CHECK_EQ(oRet, OMX_ErrorNone);

  def.nPortIndex = 1;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);
  def.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  DumpPortDefine(&def);
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, &def);
  CHECK_EQ(oRet, OMX_ErrorNone);

  OMX_AUDIO_PARAM_PCMMODETYPE pcmParams;
  InitOMXParams(&pcmParams);
  pcmParams.nPortIndex = 1;
  oRet = OMX_GetParameter(pHandle, OMX_IndexParamAudioPcm, &pcmParams);
  CHECK_EQ(oRet, OMX_ErrorNone);
  pcmParams.nChannels = numChannels;
  pcmParams.eNumData = OMX_NumericalDataSigned;
  pcmParams.bInterleaved = OMX_TRUE;
  pcmParams.nBitPerSample = 16;
  pcmParams.nSamplingRate = sampleRate;
  pcmParams.ePCMMode = OMX_AUDIO_PCMModeLinear;
  pcmParams.eChannelMapping[0]=OMX_AUDIO_ChannelCF;
  oRet = OMX_SetParameter(pHandle, OMX_IndexParamAudioPcm, &pcmParams);
  CHECK_EQ(oRet, OMX_ErrorNone);

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < 4; ++i) {
    oRet = OMX_AllocateBuffer(pHandle, &omx_ctx->inBuffer[i], 0, NULL, 8192);
    CHECK_EQ(oRet, OMX_ErrorNone);
    CHECK(omx_ctx->inBuffer[i]);
    logv("Port: %d index: %d Allocate Buffer: %p len: %d\n",
        0, i, omx_ctx->inBuffer[i]->pBuffer, omx_ctx->inBuffer[i]->nFilledLen);
  }

  for (int32_t i = 0; i < 4; ++i) {
    oRet = OMX_AllocateBuffer(pHandle, &omx_ctx->outBuffer[i], 1, NULL, 4096 << 2);
    CHECK_EQ(oRet, OMX_ErrorNone);
    CHECK(omx_ctx->outBuffer[i]);
    logv("Port: %d index: %d Allocate Buffer: %p len: %d\n",
        1, i, omx_ctx->outBuffer[i]->pBuffer, omx_ctx->outBuffer[i]->nFilledLen);
  }
  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < 4; ++i) {
    int32_t status = cirq_enqueue(omx_ctx->ebd, omx_ctx->inBuffer[i]);
    CHECK_EQ(0, status);
    status = cirq_enqueue(omx_ctx->fbd, omx_ctx->outBuffer[i]);
    CHECK_EQ(0, status);
  }

  uint32_t tid = 0;
  omx_ctx->thread->start(tid);
  os_sleep(1000);

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  for (int32_t i = 0; i < 4; ++i) {
    oRet = OMX_FreeBuffer(pHandle, 0, omx_ctx->inBuffer[i]);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }
  for (int32_t i = 0; i < 4; ++i) {
    oRet = OMX_FreeBuffer(pHandle, 1, omx_ctx->outBuffer[i]);
    CHECK_EQ(oRet, OMX_ErrorNone);
  }

  oRet = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  CHECK_EQ(oRet, OMX_ErrorNone);

  oRet = OMX_FreeHandle(pHandle);
  CHECK_EQ(oRet, OMX_ErrorNone);

  oRet = OMX_Deinit();
  CHECK_EQ(oRet, OMX_ErrorNone);

  omx_ctx->thread->stop();
  delete omx_ctx->thread;
  status = cirq_destory(omx_ctx->fbd);
  CHECK_EQ(0, status);
  status = cirq_destory(omx_ctx->ebd);
  CHECK_EQ(0, status);
  fclose(omx_ctx->fp);
  fclose(omx_ctx->fp_encoded);
  fclose(omx_ctx->fp_len);

  free(omx_ctx);
  return 0;
}

static void usage() {
  logv("Usage:\n");
  logv("./omxil_utest fun_name [options]\n");
  logv("fun_name as following:\n");
  logv("1. ve\t\tavc encoder\n");
  logv("2. vd\t\tavc decoder\n");
  logv("3. ae\t\taac encoder\n");
  logv("4. ad\t\taac decoder\n");

}

int32_t main(int32_t argc, char *argv[]) {
  if (argc < 2) {
    usage();
    exit(0);
  }
  if (!strcmp(argv[1], "ve")) {
    return ve_main(argc-2, &argv[2]);
  } else if (!strcmp(argv[1], "vd")) {
    return vd_main(argc-2, &argv[2]);
  } else if (!strcmp(argv[1], "ae")) {
    return ae_main(argc-2, &argv[2]);
  } else if (!strcmp(argv[1], "ad")) {
    return ad_main(argc-2, &argv[2]);
  } else {
    usage();
  }
  return 0;
}
