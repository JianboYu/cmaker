#ifndef SIMPLE_SOFT_OMX_COMPONENT_H_
#define SIMPLE_SOFT_OMX_COMPONENT_H_

#include <vector>
#include <list>
#include <os_mutex.h>
#include <os_cond.h>
#include <os_thread.h>
#include <core_scoped_ptr.h>
#include <core_constructor.h>
#include <soft_omx_component.h>

using namespace core;
namespace omxil {

class SimpleSoftOMXComponent : public SoftOMXComponent {
public:
    SimpleSoftOMXComponent(
            const char *name,
            const OMX_CALLBACKTYPE *callbacks,
            OMX_PTR appData,
            OMX_COMPONENTTYPE **component);

    void onMessageReceived();

protected:
    struct BufferInfo {
        OMX_BUFFERHEADERTYPE *mHeader;
        bool mOwnedByUs;
    };

    struct PortInfo {
        OMX_PARAM_PORTDEFINITIONTYPE mDef;
        std::vector<BufferInfo> mBuffers;
        std::list<BufferInfo *> mQueue;

        enum {
            NONE,
            DISABLING,
            ENABLING,
        } mTransition;
    };

    enum {
        kStoreMetaDataExtensionIndex = OMX_IndexVendorStartUnused + 1,
        kPrepareForAdaptivePlaybackIndex,
    };

    void addPort(const OMX_PARAM_PORTDEFINITIONTYPE &def);

    virtual OMX_ERRORTYPE internalGetParameter(
            OMX_INDEXTYPE index, OMX_PTR params);

    virtual OMX_ERRORTYPE internalSetParameter(
            OMX_INDEXTYPE index, const OMX_PTR params);

    virtual void onQueueFilled(OMX_U32 portIndex);
    std::list<BufferInfo *> &getPortQueue(OMX_U32 portIndex);

    virtual void onPortFlushCompleted(OMX_U32 portIndex);
    virtual void onPortEnableCompleted(OMX_U32 portIndex, bool enabled);
    virtual void onReset();

    PortInfo *editPortInfo(OMX_U32 portIndex);

private:
    struct MessageInfo {
        int32_t cmd;
        int32_t param;
        OMX_BUFFERHEADERTYPE *header;
        enum MessageType{
            kWhatSendCommand,
            kWhatEmptyThisBuffer,
            kWhatFillThisBuffer,
        }type;
    };


    scoped_ptr<os::Mutex> mLock;
    scoped_ptr<os::Mutex> mMsgLock;
    scoped_ptr<os::Cond> mMsgCond;
    scoped_ptr<os::Thread> mMsgThread;
    std::list<MessageInfo> mMsg;

    static bool MsgThread(void *ctx);
    bool MsgThread();

    OMX_STATETYPE mState;
    OMX_STATETYPE mTargetState;

    std::vector<PortInfo> mPorts;

    bool isSetParameterAllowed(
            OMX_INDEXTYPE index, const OMX_PTR params) const;

    virtual OMX_ERRORTYPE sendCommand(
            OMX_COMMANDTYPE cmd, OMX_U32 param, OMX_PTR data);

    virtual OMX_ERRORTYPE getParameter(
            OMX_INDEXTYPE index, OMX_PTR params);

    virtual OMX_ERRORTYPE setParameter(
            OMX_INDEXTYPE index, const OMX_PTR params);

    virtual OMX_ERRORTYPE useBuffer(
            OMX_BUFFERHEADERTYPE **buffer,
            OMX_U32 portIndex,
            OMX_PTR appPrivate,
            OMX_U32 size,
            OMX_U8 *ptr);

    virtual OMX_ERRORTYPE allocateBuffer(
            OMX_BUFFERHEADERTYPE **buffer,
            OMX_U32 portIndex,
            OMX_PTR appPrivate,
            OMX_U32 size);

    virtual OMX_ERRORTYPE freeBuffer(
            OMX_U32 portIndex,
            OMX_BUFFERHEADERTYPE *buffer);

    virtual OMX_ERRORTYPE emptyThisBuffer(
            OMX_BUFFERHEADERTYPE *buffer);

    virtual OMX_ERRORTYPE fillThisBuffer(
            OMX_BUFFERHEADERTYPE *buffer);

    virtual OMX_ERRORTYPE getState(OMX_STATETYPE *state);

    void onSendCommand(OMX_COMMANDTYPE cmd, OMX_U32 param);
    void onChangeState(OMX_STATETYPE state);
    void onPortEnable(OMX_U32 portIndex, bool enable);
    void onPortFlush(OMX_U32 portIndex, bool sendFlushComplete);

    void checkTransitions();

    DISALLOW_EVIL_CONSTRUCTORS(SimpleSoftOMXComponent);
};

}  // namespace omxil

#endif  // SIMPLE_SOFT_OMX_COMPONENT_H_
