#include "XLinkConnector.h"

namespace hddl {

XLinkConnector::XLinkConnector()
    : m_init(false)
{
}

int XLinkConnector::init(PipelineManager& pipeMgr)
{
    m_pipeManager = &pipeMgr;
    m_ghandler.protocol = PCIE;
    auto status = XLinkInitialize(&m_ghandler);
    if (status != X_LINK_SUCCESS)
        return -1;

    m_init = true;
    return 0;
}

void XLinkConnector::uninit()
{
    m_init = false;
    m_pipeManager = nullptr;
    XLinkUninitialize();
}

void XLinkConnector::stop()
{
    m_init = false;
}

void XLinkConnector::run()
{
    channelId_t channelId = -1;
    streamPacketDesc_t packetDesc;
    streamPacketDesc_t* packet = &packetDesc;

    while (m_init) {
        auto status = XLinkReadDataARM(&channelId, &packet);
        if (status != X_LINK_SUCCESS)
            continue;

        std::string response = generateResponse(packet);

        status = XLinkReleaseDataARM(channelId);
        if (status != X_LINK_SUCCESS)
            continue;

        if (!response.empty()) {
            status = XLinkWriteDataARM(channelId, (const uint8_t*)response.c_str(), response.length());
            if (status != X_LINK_SUCCESS)
                continue;
        }
    }
}

std::string XLinkConnector::generateResponse(streamPacketDesc_t* packet)
{
    std::string rsp;
    HalMsgRequest request;
    HalMsgResponse response;

    if (!request.ParseFromArray(packet->data, packet->length))
        return rsp;

    response.set_req_seq_no(request.req_seq_no());
    response.set_ret_code(RC_ERROR);

    switch (request.req_type()) {
    case CREATE_PIPELINE_REQUEST:
        handleCreate(request, response);
        break;
    case DESTROY_PIPELINE_REQUEST:
        handleDestroy(request, response);
        break;
    case MODIFY_PIPELINE_REQUEST:
        handleModify(request, response);
        break;
    case PLAY_PIPELINE_REQUEST:
        handlePlay(request, response);
        break;
    case STOP_PIPELINE_REQUEST:
        handleStop(request, response);
        break;
    case PAUSE_PIPELINE_REQUEST:
        handlePause(request, response);
        break;
    default:
        response.set_ret_code(RC_ERROR);
        break;
    }

    response.SerializeToString(&rsp);

    return rsp;
}

void XLinkConnector::handleCreate(HalMsgRequest& request, HalMsgResponse& response)
{
    auto status = m_pipeManager->addPipeline(request.pipeline_id(),
        request.create().launch_data(), request.create().config_data());

    response.set_rsp_type(CREATE_PIPELINE_RESPONSE);
    response.set_ret_code(mapStatus(status));
}

void XLinkConnector::handleDestroy(HalMsgRequest& request, HalMsgResponse& response)
{
    auto status = m_pipeManager->deletePipeline(request.pipeline_id());

    response.set_rsp_type(DESTROY_PIPELINE_RESPONSE);
    response.set_ret_code(mapStatus(status));
}

void XLinkConnector::handleModify(HalMsgRequest& request, HalMsgResponse& response)
{
    auto status = m_pipeManager->modifyPipeline(request.pipeline_id(),
        request.modify().config_data());

    response.set_rsp_type(MODIFY_PIPELINE_RESPONSE);
    response.set_ret_code(mapStatus(status));
}

void XLinkConnector::handlePlay(HalMsgRequest& request, HalMsgResponse& response)
{
    auto status = m_pipeManager->playPipeline(request.pipeline_id());

    response.set_rsp_type(PLAY_PIPELINE_RESPONSE);
    response.set_ret_code(mapStatus(status));
}

void XLinkConnector::handleStop(HalMsgRequest& request, HalMsgResponse& response)
{
    auto status = m_pipeManager->stopPipeline(request.pipeline_id());

    response.set_rsp_type(STOP_PIPELINE_RESPONSE);
    response.set_ret_code(mapStatus(status));
}

void XLinkConnector::handlePause(HalMsgRequest& request, HalMsgResponse& response)
{
    auto status = m_pipeManager->pausePipeline(request.pipeline_id());

    response.set_rsp_type(PAUSE_PIPELINE_RESPONSE);
    response.set_ret_code(mapStatus(status));
}

HalRetCode XLinkConnector::mapStatus(PipelineStatus status)
{
    HalRetCode rc;
    switch (status) {
    case PipelineStatus::SUCCESS:
        rc = RC_SUCCESS;
        break;
    case PipelineStatus::ERROR:
        rc = RC_ERROR;
        break;
    case PipelineStatus::COMM_TIMEOUT:
        rc = RC_COMM_TIMEOUT;
        break;
    case PipelineStatus::INVALID_PARAMETER:
        rc = RC_INVALID_PARAMETER;
        break;
    case PipelineStatus::NOT_EXIST:
        rc = RC_NOT_EXIST;
        break;
    case PipelineStatus::ALREADY_CREATED:
        rc = RC_ALREADY_CREATED;
        break;
    case PipelineStatus::ALREADY_STARTED:
        rc = RC_ALREADY_STARTED;
        break;
    case PipelineStatus::NOT_PLAYING:
        rc = RC_NOT_PLAYING;
        break;
    case PipelineStatus::STOPPED:
        rc = RC_STOPPED;
        break;
    default:
        rc = RC_ERROR;
    }

    return rc;
}

}
