#include "PipelineIpcClient.h"
#include <arpa/inet.h>

namespace hddl {

PipelineIpcClient::PipelineIpcClient(int pipeId, Socket socket)
    : m_length(0)
    , m_buffer()
    , m_socket(std::move(socket))
    , m_seq_no(0)
    , m_pipeId(pipeId)
    , m_timeout(6000)
    , m_pipe()
{
}

void PipelineIpcClient::readResponse()
{
    std::lock_guard<std::mutex> lock(m_socketMutex);
    boost::asio::async_read(
        *m_socket, boost::asio::buffer(&m_length, sizeof(m_length)),
        [this](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                std::lock_guard<std::mutex> lock(m_socketMutex);
                m_buffer = {};
                m_length = ntohl(m_length);
                boost::asio::async_read(
                    *m_socket, boost::asio::buffer(m_buffer, m_length),
                    boost::bind(&PipelineIpcClient::parseResponse, this, _1));
            }
        });
}

void PipelineIpcClient::parseResponse(boost::system::error_code ec)
{
    if (!ec) {
        std::lock_guard<std::mutex> lock(m_socketMutex);
        auto response = std::unique_ptr<MsgResponse>(new MsgResponse);
        if (response->ParseFromArray(m_buffer.data(), static_cast<int>(m_length)))
            handleResponse(std::move(response));
    }

    readResponse();
}

void PipelineIpcClient::handleResponse(std::unique_ptr<MsgResponse> response)
{
    if (!response) {
        return;
    }

    switch (response->rsp_type()) {
    case CREATE_RESPONSE ... STOP_RESPONSE: {
        auto request = fetchRequestBySeqNo(response->req_seq_no());
        if (request)
            onResponseReceived(request, std::move(response));
        break;
    }
    case METADATA_EVENT: {
        //TODO::send_matadata()
        break;
    }
    case EOS_EVENT: {
        m_pipe->setState(MPState::PIPELINE_EOS);
        m_pipe->sendEventToHost(PipelineEvent::PIPELINE_EOS);
        break;
    }
    case ERROR_EVENT: {
        m_pipe->setState(MPState::RUNTIME_ERROR);
        m_pipe->sendEventToHost(PipelineEvent::RUNTIME_ERROR);
        break;
    }
    default:
        return;
    }
}

std::shared_ptr<PipelineIpcClient::Request> PipelineIpcClient::fetchRequestBySeqNo(uint64_t seqNo)
{
    std::lock_guard<std::mutex> lock(m_requestSentMutex);
    auto it = find_if(m_reqSentList.begin(), m_reqSentList.end(),
        [seqNo](const std::shared_ptr<Request>& req) { return req->reqMsg.req_seq_no() == seqNo; });
    if (it != m_reqSentList.end()) {
        std::shared_ptr<Request> item = *it;
        m_reqSentList.erase(it);
        return item;
    } else {
        return {};
    }
}

void PipelineIpcClient::onResponseReceived(std::shared_ptr<Request>& request, std::unique_ptr<MsgResponse> response)
{
    std::unique_lock<std::mutex> lock(request->mutex);
    request->rspMsg = std::move(response);
    lock.unlock();
    request->responseReceived.notify_one();
}

std::shared_ptr<PipelineIpcClient::Request> PipelineIpcClient::createRequest(MsgReqType type)
{
    auto request = std::make_shared<Request>();
    request->reqMsg.set_req_type(type);
    request->reqMsg.set_pipeline_id(m_pipeId);
    request->reqMsg.set_req_seq_no(m_seq_no++);

    return request;
}

std::unique_ptr<MsgResponse> PipelineIpcClient::sendRequestWaitResponse(std::shared_ptr<Request>& request)
{
    if (sendRequest(request->reqMsg)) {
        std::unique_lock<std::mutex> lock(m_requestSentMutex);
        m_reqSentList.push_back(request);
        lock.unlock();
    } else {
        return {};
    }

    return waitResponse(request);
}

bool PipelineIpcClient::sendRequest(MsgRequest& request)
{
    std::lock_guard<std::mutex> lock(m_socketMutex);
    std::string buf;

    request.SerializeToString(&buf);
    auto length = htonl(static_cast<uint32_t>(buf.size()));
    std::vector<boost::asio::const_buffer> buffers;
    buffers.emplace_back(&length, sizeof(length));
    buffers.emplace_back(buf.data(), buf.size());
    boost::system::error_code ec;
    if (m_socket->write_some(buffers, ec) == 0)
        return false;

    return true;
}

std::unique_ptr<MsgResponse> PipelineIpcClient::waitResponse(std::shared_ptr<PipelineIpcClient::Request>& request)
{
    std::unique_lock<std::mutex> lock(request->mutex);
    request->responseReceived.wait_for(lock, std::chrono::milliseconds(m_timeout));

    if (request->rspMsg) {
        return std::unique_ptr<MsgResponse>(request->rspMsg.release());
    } else {
        return {};
    }
}

PipelineStatus PipelineIpcClient::create(std::string launch, std::string config)
{
    auto request = createRequest(CREATE_REQUEST);
    request->reqMsg.mutable_create()->set_launch_data(launch);
    request->reqMsg.mutable_create()->set_config_data(config);

    auto response = sendRequestWaitResponse(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIpcClient::modify(std::string config)
{
    auto request = createRequest(MODIFY_REQUEST);
    request->reqMsg.mutable_modify()->set_config_data(config);

    auto response = sendRequestWaitResponse(request);
    if (!response)
        return PipelineStatus::ERROR;
    if (response->ret_code() != 0)
        return PipelineStatus::INVALID_PARAMETER;

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIpcClient::destroy()
{
    auto request = createRequest(DESTROY_REQUEST);

    auto response = sendRequestWaitResponse(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIpcClient::play()
{
    auto request = createRequest(PLAY_REQUEST);

    auto response = sendRequestWaitResponse(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIpcClient::stop()
{
    auto request = createRequest(STOP_REQUEST);

    auto response = sendRequestWaitResponse(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIpcClient::pause()
{
    auto request = createRequest(PAUSE_REQUEST);

    auto response = sendRequestWaitResponse(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIpcClient::setChannel(const std::string& element, const int channelId)
{
    auto request = createRequest(SET_CHANNELID_REQUEST);
    request->reqMsg.mutable_set_channel()->set_element(element);
    request->reqMsg.mutable_set_channel()->set_channelid(channelId);

    auto response = sendRequestWaitResponse(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    return PipelineStatus::SUCCESS;
}
}
