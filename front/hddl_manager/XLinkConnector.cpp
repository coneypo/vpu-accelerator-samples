#include "XLinkConnector.h"
#include "PipelineManager.h"

/* Not really used in xlink simulator, place holder for now */
const uint32_t DATA_FRAGMENT_SIZE = 8192;
const uint32_t TIMEOUT = 0;

namespace hddl {

XLinkConnector::XLinkConnector()
    : m_init(false)
{
}

int XLinkConnector::init()
{
    m_pipeManager = &(PipelineManager::getInstance());
    m_ghandler.protocol = PCIE;
    m_ghandler.profEnable = 1;
    auto status = XLinkInitialize(&m_ghandler);
    if (status != X_LINK_SUCCESS)
        return -1;

    m_handler.deviceType = PCIE_DEVICE;
    m_handler.devicePath = (char*)"/tmp/xlink_mock";

    OperationMode_t operationType = RXB_TXB;

    if (XLinkOpenChannel(&m_handler, m_commChannel, operationType, DATA_FRAGMENT_SIZE, TIMEOUT) != X_LINK_SUCCESS)
        return -1;

    m_init = true;
    return 0;
}

void XLinkConnector::uninit()
{
    XLinkCloseChannel(&m_handler, m_commChannel);
    m_init = false;
    m_pipeManager = nullptr;
}

void XLinkConnector::stop()
{
    m_init = false;
}

void XLinkConnector::run()
{
    uint8_t* message = nullptr;
    uint32_t size = 0;

    while (m_init) {
        auto status = XLinkReadData(&m_handler, m_commChannel, &message, &size);
        if (status != X_LINK_SUCCESS)
            continue;

        std::string response = generateResponse(message, size);

        status = XLinkReleaseData(&m_handler, m_commChannel, message);
        if (status != X_LINK_SUCCESS)
            continue;

        if (!response.empty()) {
            std::lock_guard<std::mutex> lock(m_commChannelMutex);
            status = XLinkWriteData(&m_handler, m_commChannel, (const uint8_t*)response.c_str(), response.length());
            if (status != X_LINK_SUCCESS)
                continue;
        }
    }
}

channelId_t XLinkConnector::openXLinkChannel()
{
    std::lock_guard<std::mutex> lock(m_channelMutex);

    channelId_t channelId = 0x401;
    for (auto& it : m_channelSet) {
        if (it > channelId)
            break;
        channelId++;
    }

    OperationMode_t operationType = RXB_TXB;

    if (XLinkOpenChannel(&m_handler, channelId, operationType, DATA_FRAGMENT_SIZE, TIMEOUT) != X_LINK_SUCCESS)
        return 0;

    m_channelSet.insert(channelId);
    return channelId;
}

void XLinkConnector::closeXLinkChannel(channelId_t channelId)
{
    std::lock_guard<std::mutex> lock(m_channelMutex);

    XLinkCloseChannel(&m_handler, channelId);

    m_channelSet.erase(channelId);
}

std::string XLinkConnector::generateResponse(const uint8_t* message, uint32_t size)
{
    std::string rsp;
    HalMsgRequest request;
    HalMsgResponse response;

    if (!request.ParseFromArray(message, size))
        return rsp;

    response.set_req_seq_no(request.req_seq_no());
    response.set_pipeline_id(request.pipeline_id());
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
    case LOAD_FILE_REQUEST:
        handleLoadFile(request, response);
        break;
    case UNLOAD_FILE_REQUEST:
        handleUnloadFile(request, response);
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
    auto channelId = static_cast<int>(openXLinkChannel());

    if (channelId != 0) {
        auto status = m_pipeManager->addPipeline(channelId,
            request.create().launch_data(), request.create().config_data());

        response.set_ret_code(mapStatus(status));
    } else {
        response.set_ret_code(RC_XLINK_ERROR);
    }

    response.set_pipeline_id(channelId);
    response.set_rsp_type(CREATE_PIPELINE_RESPONSE);
}

void XLinkConnector::handleDestroy(HalMsgRequest& request, HalMsgResponse& response)
{
    auto status = m_pipeManager->deletePipeline(request.pipeline_id());

    response.set_rsp_type(DESTROY_PIPELINE_RESPONSE);
    response.set_ret_code(mapStatus(status));

    closeXLinkChannel(static_cast<channelId_t>(request.pipeline_id()));
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

void XLinkConnector::handleLoadFile(hddl::HalMsgRequest& request, hddl::HalMsgResponse& response)
{
    auto status = m_pipeManager->loadFile(request.load_file().src_data(), request.load_file().dst_path(), request.load_file().file_mode(),
        static_cast<PipelineManager::LoadFileType>(request.load_file().flag()));
    response.set_rsp_type(LOAD_FILE_RESPONSE);
    response.set_ret_code(mapStatus(status));
}

void XLinkConnector::handleUnloadFile(hddl::HalMsgRequest& request, hddl::HalMsgResponse& response)
{
    auto status = m_pipeManager->unloadFile(request.unload_file().file_path());
    response.set_rsp_type(UNLOAD_FILE_RESPONSE);
    response.set_ret_code(mapStatus(status));
}

void XLinkConnector::sendEventToHost(int id, HalMsgRspType type)
{
    std::string rsp;
    HalMsgResponse response;

    response.set_pipeline_id(id);
    response.set_ret_code(RC_SUCCESS);

    response.set_rsp_type(type);

    response.SerializeToString(&rsp);

    std::lock_guard<std::mutex> lock(m_commChannelMutex);
    XLinkWriteData(&m_handler, m_commChannel, (const uint8_t*)rsp.c_str(), rsp.length());
}

HalRetCode XLinkConnector::mapStatus(PipelineStatus status)
{
    HalRetCode rc;
    switch (status) {
    case PipelineStatus::SUCCESS:
        rc = RC_SUCCESS;
        break;
    case PipelineStatus::PIPELINE_EOS:
        rc = RC_PIPELINE_EOS;
        break;
    case PipelineStatus::RUNTIME_ERROR:
        rc = RC_RUNTIME_ERROR;
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
    case PipelineStatus::INVALID_DST_PATH:
        rc = RC_INVALID_DST_PATH;
        break;
    case PipelineStatus::FILE_ALREADY_EXIST:
        rc = RC_FILE_ALREADY_EXIST;
        break;
    default:
        rc = RC_ERROR;
    }

    return rc;
}
}
