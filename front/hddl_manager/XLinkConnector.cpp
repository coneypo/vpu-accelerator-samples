/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <algorithm>
#include <chrono>
#include <thread>

#include "PipelineManager.h"
#include "XLinkConnector.h"

/* Not really used in xlink simulator, place holder for now */
const uint32_t DATA_FRAGMENT_SIZE = 1024*1024*4;
const uint32_t TIMEOUT = 100000;

namespace hddl {

XLinkConnector::XLinkConnector()
    : m_init(false)
    , m_channelMinValue(0x402)
    , m_channelMaxValue(0xFFF)
{
}

int XLinkConnector::init()
{
    m_pipeManager = &(PipelineManager::getInstance());
    auto status = xlink_initialize();
    if (status != X_LINK_SUCCESS)
        return -1;

    m_handler.dev_type = HOST_DEVICE;
    uint32_t sw_device_id_list[20];
    uint32_t num_devices;

    int ret = xlink_get_device_list(sw_device_id_list, &num_devices);
    assert(ret == 0);
    assert(num_devices != 0);
    fprintf(stderr, "hddl_manager|XLinkConnector|Get Device Num %d \n", num_devices);
    m_handler.sw_device_id = -1;
    //Select correct device 
    for(unsigned int i = 0; i < num_devices; i++){
        fprintf(stderr, "hddl_manager|XLinkConnector|Get Device %u sw_device_id %x\n", i, sw_device_id_list[i]);
        if(sw_device_id_list[i] & (1 << 24))
            m_handler.sw_device_id = sw_device_id_list[i];
    }

    if(m_handler.sw_device_id <= 0) {
        fprintf(stderr, "hddl_manager|XLinkConnector|Cannot Get PCIE Device!\n");
        abort();
    }
    xlink_connect(&m_handler);
    xlink_opmode operationType = RXB_TXB;
    ret = xlink_close_channel(&m_handler, m_commChannel);
    fprintf(stderr, "hddl_manager|XLinkConnector|Clean command channel %d rc %d\n", m_commChannel, ret);
    fprintf(stderr, "hddl_manager|XLinkConnector|Open command channel %d\n", m_commChannel);
    while(xlink_open_channel(&m_handler, m_commChannel, operationType, DATA_FRAGMENT_SIZE, TIMEOUT) != X_LINK_SUCCESS)
    {
        int time = 5;
        fprintf(stderr, "hddl_manager|XLinkConnector|Fail to open command channel %d\n", m_commChannel);
        fprintf(stderr, "hddl_manager|XLinkConnector|Retry openchannel %d in %d seconds\n", m_commChannel, time);
        std::this_thread::sleep_for (std::chrono::seconds(time));
    }
    fprintf(stderr, "hddl_manager|XLinkConnector|Successfully open command channel %d\n", m_commChannel);

    m_init = true;
    return 0;
}

void XLinkConnector::uninit()
{
    xlink_close_channel(&m_handler, m_commChannel);
    m_init = false;
    m_pipeManager = nullptr;
}

void XLinkConnector::stop()
{
    m_init = false;
}

void XLinkConnector::run()
{
    uint8_t* message = new uint8_t[1024*1024*4];
    uint32_t size = 0;
    while (m_init) {
        auto status = xlink_read_data(&m_handler, m_commChannel, &message, &size);
        if (status != X_LINK_SUCCESS)
        {
            fprintf(stderr, "hddl_manager|XLinkConnector|xlink_read_data timeout continue to wait %d\n", status);
            continue;
        }
        std::string response = generateResponse(message, size);
        status = xlink_release_data(&m_handler, m_commChannel, NULL);
        if (status != X_LINK_SUCCESS)
        {
            fprintf(stderr, "hddl_manager|XLinkConnector|xlink_release failed rc %d\n", status);
            continue;
        }
        memset((void*)message, 0, 1024*1024*4);
        if (status != X_LINK_SUCCESS)
            continue;

        if (!response.empty()) {
            std::lock_guard<std::mutex> lock(m_commChannelMutex);
            status = xlink_write_data(&m_handler, m_commChannel, (const uint8_t*)response.c_str(), response.length());
            if (status != X_LINK_SUCCESS) {
                fprintf(stderr, "hddl_manager|XLinkConnector|xlink_release failed rc %d\n", status);
                continue;
            }
        }
    }
    delete []message;
}

std::vector<uint16_t> XLinkConnector::allocateChannel(uint32_t numChannel)
{
    if (!numChannel) {
        return {};
    }

    std::lock_guard<std::mutex> lock(m_channelMutex);

    uint16_t channel = m_channelMinValue;
    std::vector<uint16_t> channels;

    while (channel < m_channelMaxValue) {
        if (m_channelSet.count(channel)) {
            ++channel;
            continue;
        }

        channels.push_back(channel);
        ++channel;

        if (channels.size() == numChannel) {
            m_channelSet.insert(channels.begin(), channels.end());
            m_channelMinValue = channel;
            return channels;
        }
    }

    return {};
}

void XLinkConnector::deallocateChannel(const std::vector<uint16_t>& channels)
{
    if (channels.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_channelMutex);

    for (const auto& channel : channels) {
        m_channelSet.erase(channel);
    }

    auto minIter = std::min_element(channels.begin(), channels.end());
    m_channelMinValue = std::min(m_channelMinValue, *minIter);
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
    case ALLOCATE_CHANNEL_REQUEST:
        handleAllocateChannel(request, response);
        break;
    case DEALLOCATE_CHANNEL_REQUEST:
        handleDeallocateChannel(request, response);
        break;
    case SET_CHANNEL_REQUEST:
        handleSetChannel(request, response);
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
    int pipeline_id = -1;

    auto status = m_pipeManager->addPipeline(
        request.create().launch_data(), request.create().config_data(), pipeline_id);

    response.set_rsp_type(CREATE_PIPELINE_RESPONSE);
    response.set_ret_code(mapStatus(status));
    response.set_pipeline_id(pipeline_id);
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

void XLinkConnector::handleAllocateChannel(HalMsgRequest& request, HalMsgResponse& response)
{
    auto channels = allocateChannel(request.allocate_channel().num_channels());

    response.set_rsp_type(ALLOCATE_CHANNEL_RESPONSE);
    if (channels.empty()) {
        response.set_ret_code(RC_OUT_OF_CHANNEL_ID);
    } else {
        response.set_ret_code(RC_SUCCESS);
        for (auto channel : channels) {
            response.mutable_allocate_channel()->add_channelid(channel);
        }
    }
}

void XLinkConnector::handleDeallocateChannel(HalMsgRequest& request, HalMsgResponse& response)
{
    std::vector<uint16_t> channels;
    for (int i = 0; i < request.deallocate_channel().channelid_size(); ++i) {
        channels.push_back(request.deallocate_channel().channelid(i));
    }
    deallocateChannel(channels);
    response.set_rsp_type(DEALLOCATE_CHANNEL_RESPONSE);
    response.set_ret_code(RC_SUCCESS);
}

void XLinkConnector::handleSetChannel(HalMsgRequest& request, HalMsgResponse& response)
{
    auto status = m_pipeManager->setChannel(request.pipeline_id(),
        request.assign_channel().element(), request.assign_channel().channelid());
    response.set_rsp_type(SET_CHANNEL_RESPONSE);
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
    xlink_write_data(&m_handler, m_commChannel, (const uint8_t*)rsp.c_str(), rsp.length());
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
