/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef _XLINKCONNECTOR_H_
#define _XLINKCONNECTOR_H_

#include <atomic>
#include <mutex>
#include <set>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif
#include <uapi/misc/xlink_uapi.h>
#include <linux/xlink.h>
#ifdef __cplusplus
}
#endif

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
    void handleSetChannel(HalMsgRequest& request, HalMsgResponse& response);

    std::vector<xlink_channel_id_t> allocateChannel(uint32_t numChannel);
    void deallocateChannel(const std::vector<xlink_channel_id_t>&);

    PipelineManager* m_pipeManager;

    std::atomic<bool> m_init;
    xlink_handle m_handler;

    std::mutex m_channelMutex;
    xlink_channel_id_t m_channelMinValue;
    xlink_channel_id_t m_channelMaxValue;
    std::set<xlink_channel_id_t> m_channelSet;

    std::mutex m_commChannelMutex;
    const xlink_channel_id_t m_commChannel = 0x401;
};
}

#endif // _XLINKCONNECTOR_H_
