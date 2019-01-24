#ifndef _XLINKCONNECTOR_H_
#define _XLINKCONNECTOR_H_

#include <atomic>
#include <string>

#include <Xlink.h>

#include "PipelineManager.h"
#include "hal_message.pb.h"

namespace hddl {

class XLinkConnector {
public:
    XLinkConnector();
    ~XLinkConnector();

    XLinkConnector(const XLinkConnector&) = delete;
    XLinkConnector& operator=(const XLinkConnector&) = delete;

    /*
     * Return 0 if success, <0 if failed.
     */
    int init();

    /*
     * Enter infinity loop to receive messages, will return only when stop() is called.
     */
    void run();

    void stop();

private:
    void uninit();
    HalRetCode mapStatus(PipelineStatus status);

    /*
     * Return response message, empty string if error
     */
    std::string generateResponse(streamPacketDesc_t* packet);
    void handleCreate(HalMsgRequest& request, HalMsgResponse& response);
    void handleDestroy(HalMsgRequest& request, HalMsgResponse& response);
    void handleModify(HalMsgRequest& request, HalMsgResponse& response);
    void handlePlay(HalMsgRequest& request, HalMsgResponse& response);
    void handlePause(HalMsgRequest& request, HalMsgResponse& response);
    void handleStop(HalMsgRequest& request, HalMsgResponse& response);

    PipelineManager m_pipeManager;

    std::atomic<bool> m_init;
    XLinkGlobalHandler_t m_ghandler;
};

}

#endif // _XLINKCONNECTOR_H_
