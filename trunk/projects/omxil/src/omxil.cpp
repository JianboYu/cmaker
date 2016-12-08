#include <string.h>
#include <os_typedefs.h>
#include <os_mutex.h>
#include <OMX_Core.h>
#include <soft_avc_enc.h>

#ifdef __cplusplus
extern "C"
{
#endif

static const struct {
  const char *name;
  const char *suffix_name;
  const char *role;
} gs_components[] = {
  { "OMX.omxil.aac.decoder", "aacdec", "audio_decoder.aac" },
  { "OMX.omxil.aac.encoder", "aacenc", "audio_encoder.aac" },
  { "OMX.omxil.amrnb.decoder", "amrdec", "audio_decoder.amrnb" },
  { "OMX.omxil.amrnb.encoder", "amrnbenc", "audio_encoder.amrnb" },
  { "OMX.omxil.amrwb.decoder", "amrdec", "audio_decoder.amrwb" },
  { "OMX.omxil.amrwb.encoder", "amrwbenc", "audio_encoder.amrwb" },
  { "OMX.omxil.h264.decoder", "avcdec", "video_decoder.avc" },
  { "OMX.omxil.h264.encoder", "avcenc", "video_encoder.avc" },
};

os::Mutex *gs_mutex = NULL;

void __attribute__ ((constructor)) omxil_constructor(void) {
  gs_mutex = os::Mutex::Create();
}

void __attribute__ ((destructor)) omxil_destructor(void) {
  delete gs_mutex;
  gs_mutex = NULL;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Init(void) {
  os::AutoLock lock(gs_mutex);
  return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Deinit(void) {
  os::AutoLock lock(gs_mutex);
  return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentNameEnum(
                                    OMX_OUT OMX_STRING cComponentName,
                                    OMX_IN  OMX_U32 nNameLength,
                                    OMX_IN  OMX_U32 nIndex) {
  if (!cComponentName || nNameLength <= 0)
    return OMX_ErrorBadParameter;

  if (nIndex >= sizeof(gs_components)/sizeof(gs_components[0]))
    return OMX_ErrorNoMore;

  if ((OMX_U32)strlen(gs_components[nIndex].name) + 1 > nNameLength)
    return OMX_ErrorBadParameter;
  else {
    strcpy(cComponentName, (OMX_STRING)gs_components[nIndex].name);
  }
  return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetHandle(
                                    OMX_OUT OMX_HANDLETYPE* pHandle,
                                    OMX_IN  OMX_STRING cComponentName,
                                    OMX_IN  OMX_PTR pAppData,
                                    OMX_IN  OMX_CALLBACKTYPE* pCallBacks) {
  os::AutoLock lock(gs_mutex);
  OMX_COMPONENTTYPE *pComponent = NULL;
  if (!strncmp(cComponentName, "OMX.omxil.h264.encoder", 22)) {
    omxil::SoftOMXComponent *pSoftOMXCom = new omxil::SoftAVC(cComponentName,
                                                pCallBacks,
                                                pAppData,
                                                &pComponent);
    if (!pSoftOMXCom)
      return OMX_ErrorInsufficientResources;
    pComponent->pComponentPrivate = pSoftOMXCom;
    *pHandle = (OMX_HANDLETYPE)pComponent;
    return OMX_ErrorNone;
  }
  return OMX_ErrorNotImplemented;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_FreeHandle(
                                    OMX_IN  OMX_HANDLETYPE hComponent) {
  os::AutoLock lock(gs_mutex);
  OMX_COMPONENTTYPE *pComponent = (OMX_COMPONENTTYPE*)hComponent;
  omxil::SoftOMXComponent *pSoftOMXCom =
        static_cast<omxil::SoftOMXComponent*>(pComponent->pComponentPrivate);
  if (!pSoftOMXCom)
    return OMX_ErrorBadParameter;
  delete pSoftOMXCom;
  return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_SetupTunnel(
                                    OMX_IN  OMX_HANDLETYPE hOutput,
                                    OMX_IN  OMX_U32 nPortOutput,
                                    OMX_IN  OMX_HANDLETYPE hInput,
                                    OMX_IN  OMX_U32 nPortInput) {
  return OMX_ErrorNotImplemented;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_TeardownTunnel(
                                    OMX_IN  OMX_HANDLETYPE hOutput,
                                    OMX_IN  OMX_U32 nPortOutput,
                                    OMX_IN  OMX_HANDLETYPE hInput,
                                    OMX_IN  OMX_U32 nPortInput) {
  return OMX_ErrorNotImplemented;
}
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentOfRoleEnum(
  OMX_OUT OMX_STRING compName,
  OMX_IN  OMX_STRING role,
  OMX_IN  OMX_U32 nIndex) {
    return OMX_ErrorNotImplemented;
  }

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_RoleOfComponentEnum(
  OMX_OUT OMX_STRING role,
  OMX_IN  OMX_STRING compName,
  OMX_IN  OMX_U32 nIndex) {
    return OMX_ErrorNotImplemented;
  }

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetCoreInterface(
  OMX_OUT void ** ppItf,
  OMX_IN OMX_STRING cExtensionName) {
    return OMX_ErrorNotImplemented;
  }

OMX_API void OMX_APIENTRY OMX_FreeCoreInterface(
  OMX_IN void * pItf) {
  }


#ifdef __cplusplus
}
#endif
