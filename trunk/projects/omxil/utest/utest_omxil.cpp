#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG "utest_omxil"

#include <string.h>
#include <os_typedefs.h>
#include <os_assert.h>
#include <os_log.h>
#include <os_time.h>
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
    logv("EmptyBufferDone com: %p appdata: %p\n");
    return OMX_ErrorNone;
}
OMX_ERRORTYPE sFillBufferDone(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_IN OMX_PTR pAppData,
  OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) {
    logv("FillBufferDone com: %p appdata: %p\n");
    return OMX_ErrorNone;
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
  OMX_CALLBACKTYPE omx_cb;
  omx_cb.EventHandler = sEventHandler;
  omx_cb.FillBufferDone = sFillBufferDone;
  omx_cb.EmptyBufferDone = sEmptyBufferDone;
  OMX_HANDLETYPE pHandle;
  oRet = OMX_GetHandle(&pHandle,
    (OMX_STRING)"OMX.omxil.h264.encoder",
    (OMX_PTR)NULL,
    &omx_cb);
  CHECK_EQ(oRet, OMX_ErrorNone);

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

  oRet = OMX_FreeHandle(pHandle);
  CHECK_EQ(oRet, OMX_ErrorNone);

  oRet = OMX_Deinit();
  CHECK_EQ(oRet, OMX_ErrorNone);
  return 0;
}
