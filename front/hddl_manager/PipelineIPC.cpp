#include "PipelineIPC.h"
#include <arpa/inet.h>
#include <cstdio>
#include <thread>


using boost::asio::local::stream_protocol;

namespace hddl {

PipelineIPC::PipelineIPC()
    : m_ioContext()
    , m_buffer()
{
}

void PipelineIPC::init(int socketId)
{
    createSocket(socketId);
}

void PipelineIPC::uninit()
{
    m_ioContext.stop();
}

void PipelineIPC::createSocket(int socketId)
{
    if (socketId != 0)
        m_socketName += std::to_string(socketId);
    std::remove(m_socketName.c_str());
    std::thread server = std::thread(
            [this]() {
                m_acceptor = std::unique_ptr<stream_protocol::acceptor>(
                        new stream_protocol::acceptor(m_ioContext, stream_protocol::endpoint(m_socketName)));
                accept();
                m_ioContext.run();
            });
    server.detach();
}

void PipelineIPC::accept()
{
    m_tempSocket.reset(new boost::asio::local::stream_protocol::socket(m_ioContext));
    m_acceptor->async_accept(
            *m_tempSocket,
            [this](const boost::system::error_code& ec) {
                if (!ec)
                    registerPipelineConnection();
                accept();
            });
}

void PipelineIPC::registerPipelineConnection()
{
    uint32_t length;
    boost::system::error_code ec;
    if (boost::asio::read(*m_tempSocket, boost::asio::buffer(&length, sizeof(length)), ec) != sizeof(length))
        return;
    length = ntohl(length);
    if (boost::asio::read(*m_tempSocket, boost::asio::buffer(m_buffer, length), ec) != length)
        return;

    MsgResponse msg;
    if (msg.ParseFromArray(m_buffer.data(), static_cast<int>(length)) && msg.rsp_type() == REGISTER_EVENT) {
        auto id = msg.pipeline_id();
        std::lock_guard<std::mutex> mapLock(m_mapMutex);
        m_pipes[id] = std::move(m_tempSocket);
    }
}

MsgRequest PipelineIPC::createRequest(int pipeId, MsgReqType type)
{
    MsgRequest request;
    request.set_req_type(type);
    request.set_pipeline_id(pipeId);
    request.set_req_seq_no(m_seq_no[pipeId]++);

    return request;
}

std::unique_ptr<MsgResponse> PipelineIPC::sendRequest(int pipeId, const MsgRequest& request)
{
    std::string buf;

    request.SerializeToString(&buf);
    auto length = htonl(static_cast<uint32_t>(buf.size()));
    std::vector<boost::asio::const_buffer> buffers;
    buffers.emplace_back(&length, sizeof(length));
    buffers.emplace_back(buf.data(), buf.size());
    boost::system::error_code ec;
    if (m_pipes[pipeId]->write_some(buffers, ec) == 0)
        return nullptr;

    if (boost::asio::read(*m_pipes[pipeId], boost::asio::buffer(&length, sizeof(length)), ec) == 0)
        return nullptr;
    buf.resize(ntohl(length));
    if (boost::asio::read(*m_pipes[pipeId], boost::asio::buffer(&buf[0], buf.size()), ec) == 0)
        return nullptr;

    auto response = std::unique_ptr<MsgResponse>(new MsgResponse);
    if (!response->ParseFromString(buf))
        return nullptr;

    return response;
}

PipelineStatus PipelineIPC::create(int pipeId, std::string launch, std::string config)
{
    std::unique_lock<std::mutex> mapLock(m_mapMutex);

    m_seq_no[pipeId] = 0;

    if (m_pipes.find(pipeId) == m_pipes.end()) {
        m_pipes[pipeId] = nullptr;

        mapLock.unlock();
        int count = 100;
        while (!m_pipes[pipeId] && count) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (--count <= 0) {
                m_pipes.erase(pipeId);
                m_seq_no.erase(pipeId);
                return PipelineStatus::COMM_TIMEOUT;
            }
        }
        mapLock.lock();
    }

    auto request = createRequest(pipeId, CREATE_REQUEST);
    request.mutable_create()->set_launch_data(launch);
    request.mutable_create()->set_config_data(config);

    auto response = sendRequest(pipeId, request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIPC::modify(int pipeId, std::string config)
{
    std::lock_guard<std::mutex> mapLock(m_mapMutex);
    if (m_pipes.find(pipeId) == m_pipes.end())
        return PipelineStatus::NOT_EXIST;

    auto request = createRequest(pipeId, MODIFY_REQUEST);
    request.mutable_modify()->set_config_data(config);

    auto response = sendRequest(pipeId, request);
    if (!response)
        return PipelineStatus::ERROR;
    if (response->ret_code() != 0)
        return PipelineStatus::INVALID_PARAMETER;

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIPC::destroy(int pipeId)
{
    std::lock_guard<std::mutex> mapLock(m_mapMutex);
    if (m_pipes.find(pipeId) == m_pipes.end())
        return PipelineStatus::NOT_EXIST;

    auto request = createRequest(pipeId, DESTROY_REQUEST);

    auto response = sendRequest(pipeId, request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    m_pipes.erase(pipeId);
    m_seq_no.erase(pipeId);

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIPC::play(int pipeId)
{
    std::lock_guard<std::mutex> mapLock(m_mapMutex);
    if (m_pipes.find(pipeId) == m_pipes.end())
        return PipelineStatus::NOT_EXIST;

    auto request = createRequest(pipeId, PLAY_REQUEST);

    auto response = sendRequest(pipeId, request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIPC::stop(int pipeId)
{
    std::lock_guard<std::mutex> mapLock(m_mapMutex);
    if (m_pipes.find(pipeId) == m_pipes.end())
        return PipelineStatus::NOT_EXIST;

    auto request = createRequest(pipeId, STOP_REQUEST);

    auto response = sendRequest(pipeId, request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineIPC::pause(int pipeId)
{
    std::lock_guard<std::mutex> mapLock(m_mapMutex);
    if (m_pipes.find(pipeId) == m_pipes.end())
        return PipelineStatus::NOT_EXIST;

    auto request = createRequest(pipeId, PAUSE_REQUEST);

    auto response = sendRequest(pipeId, request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    return PipelineStatus::SUCCESS;
}

}
