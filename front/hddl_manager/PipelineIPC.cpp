#include "PipelineIPC.h"
#include <arpa/inet.h>
#include <cstdio>
#include <chrono>
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
        auto pipeId = msg.pipeline_id();
        auto ipcClient = PipelineIpcClient::Ptr(
                new PipelineIpcClient(pipeId, std::move(m_tempSocket)));
        std::lock_guard<std::mutex> lock(m_mapMutex);
        if (m_map.find(pipeId) == m_map.end())
            m_map[pipeId] = std::promise<PipelineIpcClient::Ptr>();
        m_map.at(pipeId).set_value(std::move(ipcClient));
    }
}

PipelineIpcClient::Ptr PipelineIPC::getIpcClient(int pipeId, int timeout_sec)
{
    std::future<PipelineIpcClient::Ptr> future;
    std::unique_lock<std::mutex> lock(m_mapMutex);
    if (m_map.find(pipeId) == m_map.end())
        m_map[pipeId] = std::promise<PipelineIpcClient::Ptr>();
    try {
        future = m_map.at(pipeId).get_future();
    } catch (...) {
        return nullptr;
    }
    lock.unlock();

    if (future.wait_for(std::chrono::seconds(timeout_sec)) != std::future_status::ready)
        return nullptr;
    return std::move(future.get());
}

void PipelineIPC::cleanupIpcClient(int pipeId)
{
    std::lock_guard<std::mutex> lock(m_mapMutex);
    m_map.erase(pipeId);
}

}
