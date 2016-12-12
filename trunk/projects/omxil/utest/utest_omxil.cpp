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
  OMX_BUFFERHEADERTYPE *inBuffer[2];
  OMX_BUFFERHEADERTYPE *outBuffer[2];
  cirq_handle fbd;
  cirq_handle ebd;
  os::Thread *thread;
  OMX_TICKS  ts;
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

bool thread_loop(void *ctx) {
  logv("thread_loop ....\n");
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
    oRet = OMX_EmptyThisBuffer(hComponent, pBuffer);
    CHECK_EQ(oRet, OMX_ErrorNone);
    logv("EmptyThisBuffer: %p\n", pBuffer);
    omx_ctx->ts += 3600;
  }

  pdata = NULL;
  cirq_dequeue(omx_ctx->fbd, &pdata);
  pBuffer = (OMX_BUFFERHEADERTYPE*)pdata;
  if (pBuffer) {
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
int32_t main(int argc, char *argv[]) {
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

  free(omx_ctx);
  return 0;
}
