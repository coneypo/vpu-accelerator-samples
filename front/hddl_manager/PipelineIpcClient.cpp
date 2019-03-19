#include "PipelineIpcClient.h"
#include <thread>

namespace hddl {

PipelineIpcClient::PipelineIpcClient(int pipeId, Socket socket)
    : m_socket(std::move(socket))
    , m_seq_no(0)
    , m_pipeId(pipeId)
{
}

MsgRequest PipelineIpcClient::createRequest(MsgReqType type)
{
    MsgRequest request;
    request.set_req_type(type);
    request.set_pipeline_id(m_pipeId);
    request.set_req_seq_no(m_seq_no++);

    return request;
}

std::unique_ptr<MsgResponse> PipelineIpcClient::sendRequest(const MsgRequest& request)
{
    std::string buf;

    request.SerializeToString(&buf);
    auto length = htonl(static_cast<uint32_t>(buf.size()));
    std::vector<boost::asio::const_buffer> buffers;
    buffers.emplace_back(&length, sizeof(length));
    buffers.emplace_back(buf.data(), buf.size());
    boost::system::error_code ec;
    if (m_socket->write_some(buffers, ec) == 0)
        return nullptr;

    if (boost::asio::read(*m_socket, boost::asio::buffer(&length, sizeof(length)), ec) == 0)
        return nullptr;
    buf.resize(ntohl(length));
    if (boost::asio::read(*m_socket, boost::asio::buffer(&buf[0], buf.size()), ec) == 0)
        return nullptr;

    auto response = std::unique_ptr<MsgResponse>(new MsgResponse);
    if (!response->ParseFromString(buf))
        return nullptr;

    return response;
}

PipelineStatus PipelineIpcClient::create(std::string launch, std::string config)
{
    auto request = createRequest(CREATE_REQUEST);
    request.mutable_create()->set_launch_data(launch);
    request.mutable_create()->set_config_data(config);

    auto response = sendRequest(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIpcClient::modify(std::string config)
{
    auto request = createRequest(MODIFY_REQUEST);
    request.mutable_modify()->set_config_data(config);

    auto response = sendRequest(request);
    if (!response)
        return PipelineStatus::ERROR;
    if (response->ret_code() != 0)
        return PipelineStatus::INVALID_PARAMETER;

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIpcClient::destroy()
{
    auto request = createRequest(DESTROY_REQUEST);

    auto response = sendRequest(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIpcClient::play()
{
    auto request = createRequest(PLAY_REQUEST);

    auto response = sendRequest(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIpcClient::stop()
{
    auto request = createRequest(STOP_REQUEST);

    auto response = sendRequest(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIpcClient::pause()
{
    auto request = createRequest(PAUSE_REQUEST);

    auto response = sendRequest(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    return PipelineStatus::SUCCESS;
}

}
