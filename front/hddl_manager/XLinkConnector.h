#ifndef _XLINKCONNECTOR_H_
#define _XLINKCONNECTOR_H_

#include <atomic>
#include <mutex>
#include <set>
#include <string>

#include <XLink.h>

#include "PipelineStatus.h"
#include "hal_message.pb.h"

namespace hddl {
class PipelineManager;

class XLinkConnector {
public:
    XLinkConnector(const XLinkConnector&) = delete;
    XLinkConnector& operator=(const XLinkConnector&) = delete;

    /*
     * Return 0 if success, <0 if failed.
     */
    int init();

    void uninit();

    /*
     * Enter infinity loop to receive messages, will return only when stop() is called.
     */
    void run();

    void stop();

    /*
     * Actively return message, including eos/runtime_error message
     */
    void sendEventToHost(int id, HalMsgRspType type);

    static XLinkConnector& getInstance()
    {
        static XLinkConnector instance;
        return instance;
    }

private:
    XLinkConnector();
    ~XLinkConnector() = default;

    HalRetCode mapStatus(PipelineStatus status);

    /*
     * Return response message, empty string if error
     */
    std::string generateResponse(const uint8_t* message, uint32_t size);
    void handleCreate(HalMsgRequest& request, HalMsgResponse& response);
    void handleDestroy(HalMsgRequest& request, HalMsgResponse& response);
    void handleModify(HalMsgRequest& request, HalMsgResponse& response);
    void handlePlay(HalMsgRequest& request, HalMsgResponse& response);
    void handlePause(HalMsgRequest& request, HalMsgResponse& response);
    void handleStop(HalMsgRequest& request, HalMsgResponse& response);
    void handleLoadFile(HalMsgRequest& request, HalMsgResponse& response);
    void handleUnloadFile(HalMsgRequest& request, HalMsgResponse& response);
    void handleAllocateChannel(HalMsgRequest& request, HalMsgResponse& response);
    void handleDeallocateChannel(HalMsgRequest& request, HalMsgResponse& response);

    channelId_t openXLinkChannel();
    void closeXLinkChannel(channelId_t channelId);
    std::vector<channelId_t> allocateChannel(uint32_t numChannel);
    void deallocateChannel(const std::vector<channelId_t>&);

    PipelineManager* m_pipeManager;

    std::atomic<bool> m_init;
    XLinkGlobalHandler_t m_ghandler;
    XLinkHandler_t m_handler;

    std::mutex m_channelMutex;
    channelId_t m_channelMinValue;
    channelId_t m_channelMaxValue;
    std::set<channelId_t> m_channelSet;

    std::mutex m_commChannelMutex;
    const channelId_t m_commChannel = 0x400;
};
}

#endif // _XLINKCONNECTOR_H_
