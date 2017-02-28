#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG "SimpleSoftOMXComponent"

#include <string.h>
#include <os_assert.h>
#include <os_log.h>
#include "simple_soft_omx_component.h"

using namespace os;
using namespace core;

namespace omxil {

SimpleSoftOMXComponent::SimpleSoftOMXComponent(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component)
    : SoftOMXComponent(name, callbacks, appData, component),
      mState(OMX_StateLoaded),
      mTargetState(OMX_StateLoaded) {
  mLock.reset(Mutex::Create());
  mMsgLock.reset(Mutex::Create());
  mMsgCond.reset(Cond::Create());
  mMsgThread.reset(Thread::Create(MsgThread, this));
  mMsgStarted = mMsgThread->start(mMsgThreadTid);
}

SimpleSoftOMXComponent::~SimpleSoftOMXComponent() {
  if (mMsgStarted) {
    mMsgThread->stop();
    mMsgStarted = false;
  }
}

OMX_ERRORTYPE SimpleSoftOMXComponent::sendCommand(
        OMX_COMMANDTYPE cmd, OMX_U32 param, OMX_PTR data) {
    AutoLock lock(mMsgLock.get());
    MessageInfo msg;
    msg.type = MessageInfo::kWhatSendCommand;
    msg.cmd = cmd;
    msg.param = param;
    mMsg.push_back(msg);
    mMsgCond->signal();

    return OMX_ErrorNone;
}

bool SimpleSoftOMXComponent::isSetParameterAllowed(
        OMX_INDEXTYPE index, const OMX_PTR params) const {
    if (mState == OMX_StateLoaded) {
        return true;
    }

    OMX_U32 portIndex;

    switch (index) {
        case OMX_IndexParamPortDefinition:
        {
            portIndex = ((OMX_PARAM_PORTDEFINITIONTYPE *)params)->nPortIndex;
            break;
        }

        case OMX_IndexParamAudioPcm:
        {
            portIndex = ((OMX_AUDIO_PARAM_PCMMODETYPE *)params)->nPortIndex;
            break;
        }

        case OMX_IndexParamAudioAac:
        {
            portIndex = ((OMX_AUDIO_PARAM_AACPROFILETYPE *)params)->nPortIndex;
            break;
        }

        default:
            return false;
    }

    CHECK(portIndex < mPorts.size());

    return !mPorts.at(portIndex).mDef.bEnabled;
}

OMX_ERRORTYPE SimpleSoftOMXComponent::getParameter(
        OMX_INDEXTYPE index, OMX_PTR params) {
    AutoLock lock(mLock.get());
    return internalGetParameter(index, params);
}

OMX_ERRORTYPE SimpleSoftOMXComponent::setParameter(
        OMX_INDEXTYPE index, const OMX_PTR params) {
    AutoLock lock(mLock.get());

    CHECK(isSetParameterAllowed(index, params));

    return internalSetParameter(index, params);
}

OMX_ERRORTYPE SimpleSoftOMXComponent::internalGetParameter(
        OMX_INDEXTYPE index, OMX_PTR params) {
    switch (index) {
        case OMX_IndexParamPortDefinition:
        {
            OMX_PARAM_PORTDEFINITIONTYPE *defParams =
                (OMX_PARAM_PORTDEFINITIONTYPE *)params;

            if (defParams->nPortIndex >= mPorts.size()
                    || defParams->nSize
                            != sizeof(OMX_PARAM_PORTDEFINITIONTYPE)) {
                return OMX_ErrorUndefined;
            }

            const PortInfo *port =
                &mPorts.at(defParams->nPortIndex);

            memcpy(defParams, &port->mDef, sizeof(port->mDef));

            return OMX_ErrorNone;
        }

        default:
            return OMX_ErrorUnsupportedIndex;
    }
}

OMX_ERRORTYPE SimpleSoftOMXComponent::internalSetParameter(
        OMX_INDEXTYPE index, const OMX_PTR params) {
    switch (index) {
        case OMX_IndexParamPortDefinition:
        {
            OMX_PARAM_PORTDEFINITIONTYPE *defParams =
                (OMX_PARAM_PORTDEFINITIONTYPE *)params;

            if (defParams->nPortIndex >= mPorts.size()) {
                return OMX_ErrorBadPortIndex;
            }
            if (defParams->nSize != sizeof(OMX_PARAM_PORTDEFINITIONTYPE)) {
                return OMX_ErrorUnsupportedSetting;
            }

            PortInfo *port =
                &mPorts.at(defParams->nPortIndex);

            // default behavior is that we only allow buffer size to increase
            if (defParams->nBufferSize > port->mDef.nBufferSize) {
                port->mDef.nBufferSize = defParams->nBufferSize;
            }

            if (defParams->nBufferCountActual < port->mDef.nBufferCountMin) {
                logw("component requires at least %u buffers (%u requested)",
                        port->mDef.nBufferCountMin, defParams->nBufferCountActual);
                return OMX_ErrorUnsupportedSetting;
            }

            port->mDef.nBufferCountActual = defParams->nBufferCountActual;
            return OMX_ErrorNone;
        }

        default:
            return OMX_ErrorUnsupportedIndex;
    }
}

OMX_ERRORTYPE SimpleSoftOMXComponent::useBuffer(
        OMX_BUFFERHEADERTYPE **header,
        OMX_U32 portIndex,
        OMX_PTR appPrivate,
        OMX_U32 size,
        OMX_U8 *ptr) {
    AutoLock lock(mLock.get());
    CHECK_LT(portIndex, mPorts.size());

    *header = new OMX_BUFFERHEADERTYPE;
    (*header)->nSize = sizeof(OMX_BUFFERHEADERTYPE);
    (*header)->nVersion.s.nVersionMajor = 1;
    (*header)->nVersion.s.nVersionMinor = 0;
    (*header)->nVersion.s.nRevision = 0;
    (*header)->nVersion.s.nStep = 0;
    (*header)->pBuffer = ptr;
    (*header)->nAllocLen = size;
    (*header)->nFilledLen = 0;
    (*header)->nOffset = 0;
    (*header)->pAppPrivate = appPrivate;
    (*header)->pPlatformPrivate = NULL;
    (*header)->pInputPortPrivate = NULL;
    (*header)->pOutputPortPrivate = NULL;
    (*header)->hMarkTargetComponent = NULL;
    (*header)->pMarkData = NULL;
    (*header)->nTickCount = 0;
    (*header)->nTimeStamp = 0;
    (*header)->nFlags = 0;
    (*header)->nOutputPortIndex = portIndex;
    (*header)->nInputPortIndex = portIndex;

    PortInfo *port = &mPorts.at(portIndex);

    CHECK(mState == OMX_StateLoaded || port->mDef.bEnabled == OMX_FALSE);

    CHECK_LT(port->mBuffers.size(), port->mDef.nBufferCountActual);

    BufferInfo buffer;
    buffer.mHeader = *header;
    buffer.mOwnedByUs = false;
    port->mBuffers.push_back(buffer);
    logi("Port[%d] buffer count: %d port need actual: %d\n", portIndex,
          port->mBuffers.size(), port->mDef.nBufferCountActual);
    if (port->mBuffers.size() == port->mDef.nBufferCountActual) {
        port->mDef.bPopulated = OMX_TRUE;
        checkTransitions();
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SimpleSoftOMXComponent::allocateBuffer(
        OMX_BUFFERHEADERTYPE **header,
        OMX_U32 portIndex,
        OMX_PTR appPrivate,
        OMX_U32 size) {
    OMX_U8 *ptr = new OMX_U8[size];

    OMX_ERRORTYPE err =
        useBuffer(header, portIndex, appPrivate, size, ptr);

    if (err != OMX_ErrorNone) {
        delete[] ptr;
        ptr = NULL;

        return err;
    }

    CHECK(!(*header)->pPlatformPrivate);
    (*header)->pPlatformPrivate = ptr;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SimpleSoftOMXComponent::freeBuffer(
        OMX_U32 portIndex,
        OMX_BUFFERHEADERTYPE *header) {
    AutoLock lock(mLock.get());

    CHECK_LT(portIndex, mPorts.size());

    PortInfo *port = &mPorts.at(portIndex);

#if 0 // XXX
    CHECK((mState == OMX_StateIdle && mTargetState == OMX_StateLoaded)
            || port->mDef.bEnabled == OMX_FALSE);
#endif

    bool found = false;
    for (size_t i = 0; i < port->mBuffers.size(); ++i) {
        BufferInfo *buffer = &port->mBuffers.at(i);

        if (buffer->mHeader == header) {
            CHECK(!buffer->mOwnedByUs);

            if (header->pPlatformPrivate != NULL) {
                // This buffer's data was allocated by us.
                CHECK_EQ(header->pPlatformPrivate, header->pBuffer);

                delete[] header->pBuffer;
                header->pBuffer = NULL;
            }

            delete header;
            header = NULL;

            port->mBuffers.erase(port->mBuffers.begin() + i);
            port->mDef.bPopulated = OMX_FALSE;

            checkTransitions();

            found = true;
            break;
        }
    }

    CHECK(found);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SimpleSoftOMXComponent::emptyThisBuffer(
        OMX_BUFFERHEADERTYPE *buffer) {
    AutoLock lock(mMsgLock.get());
    MessageInfo msg;
    msg.type = MessageInfo::kWhatEmptyThisBuffer;
    msg.header = buffer;
    mMsg.push_back(msg);
    mMsgCond->signal();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SimpleSoftOMXComponent::fillThisBuffer(
        OMX_BUFFERHEADERTYPE *buffer) {
    AutoLock lock(mMsgLock.get());
    MessageInfo msg;
    msg.type = MessageInfo::kWhatFillThisBuffer;
    msg.header = buffer;
    mMsg.push_back(msg);
    mMsgCond->signal();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SimpleSoftOMXComponent::getState(OMX_STATETYPE *state) {
    AutoLock lock(mLock.get());

    *state = mState;

    return OMX_ErrorNone;
}

void SimpleSoftOMXComponent::onMessageReceived() {
    CHECK_GT(mMsg.size(), (uint32_t)0);
    MessageInfo msg = mMsg.front();
    mMsg.pop_front();

    switch (msg.type) {
        case MessageInfo::kWhatSendCommand:
        {
            onSendCommand((OMX_COMMANDTYPE)msg.cmd, (OMX_U32)msg.param);
            break;
        }

        case MessageInfo::kWhatEmptyThisBuffer:
        case MessageInfo::kWhatFillThisBuffer:
        {
            OMX_BUFFERHEADERTYPE *header = msg.header;
            CHECK(mState == OMX_StateExecuting && mTargetState == mState);

            bool found = false;
            size_t portIndex = (MessageInfo::kWhatEmptyThisBuffer == msg.type)?
                    header->nInputPortIndex: header->nOutputPortIndex;
            PortInfo *port = &mPorts.at(portIndex);

            for (size_t j = 0; j < port->mBuffers.size(); ++j) {
                BufferInfo *buffer = &port->mBuffers.at(j);

                if (buffer->mHeader == header) {
                    CHECK(!buffer->mOwnedByUs);

                    buffer->mOwnedByUs = true;

                    if (!((msg.type == MessageInfo::kWhatEmptyThisBuffer
                            && port->mDef.eDir == OMX_DirInput)
                            || (port->mDef.eDir == OMX_DirOutput))) {
                        CHECK(!"BUG");
                    }

                    port->mQueue.push_back(buffer);
                    /*logv("portIndex: %d buffer->mHeader: %p pBuffer: %p mQueue size: %d\n",
                          portIndex, buffer->mHeader, buffer->mHeader->pBuffer, port->mQueue.size());*/
                    onQueueFilled(portIndex);

                    found = true;
                    break;
                }
            }

            CHECK(found);
            break;
        }

        default:
            CHECK(NULL);
            break;
    }
}

void SimpleSoftOMXComponent::onSendCommand(
        OMX_COMMANDTYPE cmd, OMX_U32 param) {
    switch (cmd) {
        case OMX_CommandStateSet:
        {
            onChangeState((OMX_STATETYPE)param);
            break;
        }

        case OMX_CommandPortEnable:
        case OMX_CommandPortDisable:
        {
            onPortEnable(param, cmd == OMX_CommandPortEnable);
            break;
        }

        case OMX_CommandFlush:
        {
            onPortFlush(param, true /* sendFlushComplete */);
            break;
        }

        default:
            CHECK(NULL);
            break;
    }
}

void SimpleSoftOMXComponent::onChangeState(OMX_STATETYPE state) {
    // We shouldn't be in a state transition already.
    CHECK_EQ((int)mState, (int)mTargetState);
    logv("onChangeState new state: %d\n", state);
    switch (mState) {
        case OMX_StateLoaded:
            CHECK_EQ((int)state, (int)OMX_StateIdle);
            break;
        case OMX_StateIdle:
            CHECK((state == OMX_StateLoaded) || (state == OMX_StateExecuting));
            break;
        case OMX_StateExecuting:
        {
            CHECK_EQ((int)state, (int)OMX_StateIdle);

            for (size_t i = 0; i < mPorts.size(); ++i) {
                onPortFlush(i, false /* sendFlushComplete */);
            }

            mState = OMX_StateIdle;
            notify(OMX_EventCmdComplete, OMX_CommandStateSet, state, NULL);
            break;
        }

        default:
            CHECK(NULL);
    }

    mTargetState = state;

    checkTransitions();
}

void SimpleSoftOMXComponent::onReset() {
    // no-op
}

void SimpleSoftOMXComponent::onPortEnable(OMX_U32 portIndex, bool enable) {
    CHECK_LT(portIndex, mPorts.size());

    PortInfo *port = &mPorts.at(portIndex);
    CHECK_EQ((int)port->mTransition, (int)PortInfo::NONE);
    CHECK(port->mDef.bEnabled == !enable);

    if (!enable) {
        port->mDef.bEnabled = OMX_FALSE;
        port->mTransition = PortInfo::DISABLING;

        for (size_t i = 0; i < port->mBuffers.size(); ++i) {
            BufferInfo *buffer = &port->mBuffers.at(i);

            if (buffer->mOwnedByUs) {
                buffer->mOwnedByUs = false;

                if (port->mDef.eDir == OMX_DirInput) {
                    notifyEmptyBufferDone(buffer->mHeader);
                } else {
                    CHECK_EQ(port->mDef.eDir, OMX_DirOutput);
                    notifyFillBufferDone(buffer->mHeader);
                }
            }
        }

        port->mQueue.clear();
    } else {
        port->mTransition = PortInfo::ENABLING;
    }

    checkTransitions();
}

void SimpleSoftOMXComponent::onPortFlush(
        OMX_U32 portIndex, bool sendFlushComplete) {
    if (portIndex == OMX_ALL) {
        for (size_t i = 0; i < mPorts.size(); ++i) {
            onPortFlush(i, sendFlushComplete);
        }

        if (sendFlushComplete) {
            notify(OMX_EventCmdComplete, OMX_CommandFlush, OMX_ALL, NULL);
        }

        return;
    }

    CHECK_LT(portIndex, mPorts.size());

    PortInfo *port = &mPorts.at(portIndex);
    CHECK_EQ((int)port->mTransition, (int)PortInfo::NONE);

    for (size_t i = 0; i < port->mBuffers.size(); ++i) {
        BufferInfo *buffer = &port->mBuffers.at(i);

        if (!buffer->mOwnedByUs) {
            continue;
        }

        buffer->mHeader->nFilledLen = 0;
        buffer->mHeader->nOffset = 0;
        buffer->mHeader->nFlags = 0;

        buffer->mOwnedByUs = false;

        if (port->mDef.eDir == OMX_DirInput) {
            notifyEmptyBufferDone(buffer->mHeader);
        } else {
            CHECK_EQ(port->mDef.eDir, OMX_DirOutput);

            notifyFillBufferDone(buffer->mHeader);
        }
    }

    port->mQueue.clear();

    if (sendFlushComplete) {
        notify(OMX_EventCmdComplete, OMX_CommandFlush, portIndex, NULL);

        onPortFlushCompleted(portIndex);
    }
}

void SimpleSoftOMXComponent::checkTransitions() {
    logv("mState: %d mTargetState: %d\n", mState, mTargetState);
    if (mState != mTargetState) {
        bool transitionComplete = true;

        if (mState == OMX_StateLoaded) {
            CHECK_EQ((int)mTargetState, (int)OMX_StateIdle);

            for (size_t i = 0; i < mPorts.size(); ++i) {
                const PortInfo &port = mPorts.at(i);
                if (port.mDef.bEnabled == OMX_FALSE) {
                    continue;
                }

                if (port.mDef.bPopulated == OMX_FALSE) {
                    transitionComplete = false;
                    break;
                }
            }
        } else if (mTargetState == OMX_StateLoaded) {
            CHECK_EQ((int)mState, (int)OMX_StateIdle);

            for (size_t i = 0; i < mPorts.size(); ++i) {
                const PortInfo &port = mPorts.at(i);
                if (port.mDef.bEnabled == OMX_FALSE) {
                    continue;
                }

                size_t n = port.mBuffers.size();

                if (n > 0) {
                    CHECK_LE(n, port.mDef.nBufferCountActual);

                    if (n == port.mDef.nBufferCountActual) {
                        CHECK_EQ((int)port.mDef.bPopulated, (int)OMX_TRUE);
                    } else {
                        CHECK_EQ((int)port.mDef.bPopulated, (int)OMX_FALSE);
                    }

                    transitionComplete = false;
                    break;
                }
            }
        }

        if (transitionComplete) {
            mState = mTargetState;

            if (mState == OMX_StateLoaded) {
                onReset();
            }

            notify(OMX_EventCmdComplete, OMX_CommandStateSet, mState, NULL);
        }
    }

    for (size_t i = 0; i < mPorts.size(); ++i) {
        PortInfo *port = &mPorts.at(i);

        if (port->mTransition == PortInfo::DISABLING) {
            if (port->mBuffers.empty()) {
                logv("Port %d now disabled.\n", i);

                port->mTransition = PortInfo::NONE;
                notify(OMX_EventCmdComplete, OMX_CommandPortDisable, i, NULL);

                onPortEnableCompleted(i, false /* enabled */);
            }
        } else if (port->mTransition == PortInfo::ENABLING) {
            if (port->mDef.bPopulated == OMX_TRUE) {
                logv("Port %d now enabled.\n", i);

                port->mTransition = PortInfo::NONE;
                port->mDef.bEnabled = OMX_TRUE;
                notify(OMX_EventCmdComplete, OMX_CommandPortEnable, i, NULL);

                onPortEnableCompleted(i, true /* enabled */);
            }
        }
    }
}

void SimpleSoftOMXComponent::addPort(const OMX_PARAM_PORTDEFINITIONTYPE &def) {
    CHECK_EQ(def.nPortIndex, mPorts.size());

    PortInfo info;
    info.mDef = def;
    info.mTransition = PortInfo::NONE;
    mPorts.push_back(info);
}

void SimpleSoftOMXComponent::onQueueFilled(OMX_U32 portIndex) {
}

void SimpleSoftOMXComponent::onPortFlushCompleted(OMX_U32 portIndex) {
}

void SimpleSoftOMXComponent::onPortEnableCompleted(
        OMX_U32 portIndex, bool enabled) {
}

std::list<SimpleSoftOMXComponent::BufferInfo *> &
SimpleSoftOMXComponent::getPortQueue(OMX_U32 portIndex) {
    CHECK_LT(portIndex, mPorts.size());
    return mPorts.at(portIndex).mQueue;
}

SimpleSoftOMXComponent::PortInfo *SimpleSoftOMXComponent::editPortInfo(
        OMX_U32 portIndex) {
    CHECK_LT(portIndex, mPorts.size());
    return &mPorts.at(portIndex);
}
//static
bool SimpleSoftOMXComponent::MsgThread(void *ctx) {
  CHECK(ctx);
  SimpleSoftOMXComponent *me = static_cast<SimpleSoftOMXComponent*>(ctx);
  return me->MsgThread();
}

bool SimpleSoftOMXComponent::MsgThread() {
  AutoLock lock(mMsgLock.get());
  if(mMsg.size() == 0) {
    if (mMsgCond->wait_timeout(mMsgLock.get(), 60))
      return true;
  }

  //BUG: fix me for size == 0 case
  if(mMsg.size() <= 0)
    return true;

  onMessageReceived();
  return true;
}

}  // namespace omxil
