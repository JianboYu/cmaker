#include <string>
#include "soft_omx_component.h"
namespace omxil {

SoftOMXComponent::SoftOMXComponent(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component)
    : mName(name),
      mCallbacks(callbacks),
      mComponent(new OMX_COMPONENTTYPE) {
    mComponent->nSize = sizeof(*mComponent);
    mComponent->nVersion.s.nVersionMajor = 1;
    mComponent->nVersion.s.nVersionMinor = 0;
    mComponent->nVersion.s.nRevision = 0;
    mComponent->nVersion.s.nStep = 0;
    mComponent->pComponentPrivate = this;
    mComponent->pApplicationPrivate = appData;

    mComponent->GetComponentVersion = NULL;
    mComponent->SendCommand = SendCommandWrapper;
    mComponent->GetParameter = GetParameterWrapper;
    mComponent->SetParameter = SetParameterWrapper;
    mComponent->GetConfig = GetConfigWrapper;
    mComponent->SetConfig = SetConfigWrapper;
    mComponent->GetExtensionIndex = GetExtensionIndexWrapper;
    mComponent->GetState = GetStateWrapper;
    mComponent->ComponentTunnelRequest = NULL;
    mComponent->UseBuffer = UseBufferWrapper;
    mComponent->AllocateBuffer = AllocateBufferWrapper;
    mComponent->FreeBuffer = FreeBufferWrapper;
    mComponent->EmptyThisBuffer = EmptyThisBufferWrapper;
    mComponent->FillThisBuffer = FillThisBufferWrapper;
    mComponent->SetCallbacks = NULL;
    mComponent->ComponentDeInit = NULL;
    mComponent->UseEGLImage = NULL;
    mComponent->ComponentRoleEnum = NULL;

    *component = mComponent;
}

SoftOMXComponent::~SoftOMXComponent() {
    delete mComponent;
    mComponent = NULL;
}

const char *SoftOMXComponent::name() const {
    return mName.c_str();
}

void SoftOMXComponent::notify(
        OMX_EVENTTYPE event,
        OMX_U32 data1, OMX_U32 data2, OMX_PTR data) {
    (*mCallbacks->EventHandler)(
            mComponent,
            mComponent->pApplicationPrivate,
            event,
            data1,
            data2,
            data);
}

void SoftOMXComponent::notifyEmptyBufferDone(OMX_BUFFERHEADERTYPE *header) {
    (*mCallbacks->EmptyBufferDone)(
            mComponent, mComponent->pApplicationPrivate, header);
}

void SoftOMXComponent::notifyFillBufferDone(OMX_BUFFERHEADERTYPE *header) {
    (*mCallbacks->FillBufferDone)(
            mComponent, mComponent->pApplicationPrivate, header);
}

// static
OMX_ERRORTYPE SoftOMXComponent::SendCommandWrapper(
        OMX_HANDLETYPE component,
        OMX_COMMANDTYPE cmd,
        OMX_U32 param,
        OMX_PTR data) {
    SoftOMXComponent *me =
        (SoftOMXComponent *)
            ((OMX_COMPONENTTYPE *)component)->pComponentPrivate;

    return me->sendCommand(cmd, param, data);
}

// static
OMX_ERRORTYPE SoftOMXComponent::GetParameterWrapper(
        OMX_HANDLETYPE component,
        OMX_INDEXTYPE index,
        OMX_PTR params) {
    SoftOMXComponent *me =
        (SoftOMXComponent *)
            ((OMX_COMPONENTTYPE *)component)->pComponentPrivate;

    return me->getParameter(index, params);
}

// static
OMX_ERRORTYPE SoftOMXComponent::SetParameterWrapper(
        OMX_HANDLETYPE component,
        OMX_INDEXTYPE index,
        OMX_PTR params) {
    SoftOMXComponent *me =
        (SoftOMXComponent *)
            ((OMX_COMPONENTTYPE *)component)->pComponentPrivate;

    return me->setParameter(index, params);
}

// static
OMX_ERRORTYPE SoftOMXComponent::GetConfigWrapper(
        OMX_HANDLETYPE component,
        OMX_INDEXTYPE index,
        OMX_PTR params) {
    SoftOMXComponent *me =
        (SoftOMXComponent *)
            ((OMX_COMPONENTTYPE *)component)->pComponentPrivate;

    return me->getConfig(index, params);
}

// static
OMX_ERRORTYPE SoftOMXComponent::SetConfigWrapper(
        OMX_HANDLETYPE component,
        OMX_INDEXTYPE index,
        OMX_PTR params) {
    SoftOMXComponent *me =
        (SoftOMXComponent *)
            ((OMX_COMPONENTTYPE *)component)->pComponentPrivate;

    return me->setConfig(index, params);
}

// static
OMX_ERRORTYPE SoftOMXComponent::GetExtensionIndexWrapper(
        OMX_HANDLETYPE component,
        OMX_STRING name,
        OMX_INDEXTYPE *index) {
    SoftOMXComponent *me =
        (SoftOMXComponent *)
            ((OMX_COMPONENTTYPE *)component)->pComponentPrivate;

    return me->getExtensionIndex(name, index);
}

// static
OMX_ERRORTYPE SoftOMXComponent::UseBufferWrapper(
        OMX_HANDLETYPE component,
        OMX_BUFFERHEADERTYPE **buffer,
        OMX_U32 portIndex,
        OMX_PTR appPrivate,
        OMX_U32 size,
        OMX_U8 *ptr) {
    SoftOMXComponent *me =
        (SoftOMXComponent *)
            ((OMX_COMPONENTTYPE *)component)->pComponentPrivate;

    return me->useBuffer(buffer, portIndex, appPrivate, size, ptr);
}

// static
OMX_ERRORTYPE SoftOMXComponent::AllocateBufferWrapper(
        OMX_HANDLETYPE component,
        OMX_BUFFERHEADERTYPE **buffer,
        OMX_U32 portIndex,
        OMX_PTR appPrivate,
        OMX_U32 size) {
    SoftOMXComponent *me =
        (SoftOMXComponent *)
            ((OMX_COMPONENTTYPE *)component)->pComponentPrivate;

    return me->allocateBuffer(buffer, portIndex, appPrivate, size);
}

// static
OMX_ERRORTYPE SoftOMXComponent::FreeBufferWrapper(
        OMX_HANDLETYPE component,
        OMX_U32 portIndex,
        OMX_BUFFERHEADERTYPE *buffer) {
    SoftOMXComponent *me =
        (SoftOMXComponent *)
            ((OMX_COMPONENTTYPE *)component)->pComponentPrivate;

    return me->freeBuffer(portIndex, buffer);
}

// static
OMX_ERRORTYPE SoftOMXComponent::EmptyThisBufferWrapper(
        OMX_HANDLETYPE component,
        OMX_BUFFERHEADERTYPE *buffer) {
    SoftOMXComponent *me =
        (SoftOMXComponent *)
            ((OMX_COMPONENTTYPE *)component)->pComponentPrivate;

    return me->emptyThisBuffer(buffer);
}

// static
OMX_ERRORTYPE SoftOMXComponent::FillThisBufferWrapper(
        OMX_HANDLETYPE component,
        OMX_BUFFERHEADERTYPE *buffer) {
    SoftOMXComponent *me =
        (SoftOMXComponent *)
            ((OMX_COMPONENTTYPE *)component)->pComponentPrivate;

    return me->fillThisBuffer(buffer);
}

// static
OMX_ERRORTYPE SoftOMXComponent::GetStateWrapper(
        OMX_HANDLETYPE component,
        OMX_STATETYPE *state) {
    SoftOMXComponent *me =
        (SoftOMXComponent *)
            ((OMX_COMPONENTTYPE *)component)->pComponentPrivate;

    return me->getState(state);
}

////////////////////////////////////////////////////////////////////////////////

OMX_ERRORTYPE SoftOMXComponent::sendCommand(
        OMX_COMMANDTYPE /* cmd */, OMX_U32 /* param */, OMX_PTR /* data */) {
    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE SoftOMXComponent::getParameter(
        OMX_INDEXTYPE /* index */, OMX_PTR /* params */) {
    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE SoftOMXComponent::setParameter(
        OMX_INDEXTYPE /* index */, const OMX_PTR /* params */) {
    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE SoftOMXComponent::getConfig(
        OMX_INDEXTYPE /* index */, OMX_PTR /* params */) {
    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE SoftOMXComponent::setConfig(
        OMX_INDEXTYPE /* index */, const OMX_PTR /* params */) {
    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE SoftOMXComponent::getExtensionIndex(
        const char * /* name */, OMX_INDEXTYPE * /* index */) {
    return OMX_ErrorUnsupportedIndex;
}

OMX_ERRORTYPE SoftOMXComponent::useBuffer(
        OMX_BUFFERHEADERTYPE ** /* buffer */,
        OMX_U32 /* portIndex */,
        OMX_PTR /* appPrivate */,
        OMX_U32 /* size */,
        OMX_U8 * /* ptr */) {
    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE SoftOMXComponent::allocateBuffer(
        OMX_BUFFERHEADERTYPE ** /* buffer */,
        OMX_U32 /* portIndex */,
        OMX_PTR /* appPrivate */,
        OMX_U32 /* size */) {
    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE SoftOMXComponent::freeBuffer(
        OMX_U32 /* portIndex */,
        OMX_BUFFERHEADERTYPE * /* buffer */) {
    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE SoftOMXComponent::emptyThisBuffer(
        OMX_BUFFERHEADERTYPE * /* buffer */) {
    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE SoftOMXComponent::fillThisBuffer(
        OMX_BUFFERHEADERTYPE * /* buffer */) {
    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE SoftOMXComponent::getState(OMX_STATETYPE * /* state */) {
    return OMX_ErrorUndefined;
}

}  // namespace omxil
