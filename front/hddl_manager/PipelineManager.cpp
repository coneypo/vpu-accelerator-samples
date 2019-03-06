#include <arpa/inet.h>
#include <cstdio>
#include <thread>

#include "PipelineManager.h"
#include "hddl_message.pb.h"

using boost::asio::local::stream_protocol;

namespace hddl {

PipelineManager::PipelineManager()
    : m_ioContext()
{
}

void PipelineManager::init(int socketId)
{
    createSocket(socketId);
}

void PipelineManager::uninit()
{
    removeAll();
    m_ioContext.stop();
}

PipelineStatus PipelineManager::removeAll()
{
    std::lock_guard<std::mutex> lock(m_mapMutex);

    m_map.clear();

    return PipelineStatus::SUCCESS;
}

std::vector<int> PipelineManager::getAll()
{
    std::lock_guard<std::mutex> lock(m_mapMutex);
    std::vector<int> ret;
    for (const auto& it : m_map)
        ret.push_back(it.first);
    return ret;
}

PipelineStatus PipelineManager::addPipeline(int id, std::string launch, std::string config)
{
    std::unique_lock<std::mutex> lock(m_mapMutex);

    if (m_map.find(id) != m_map.end())
        return PipelineStatus::ALREADY_CREATED;

    m_map.emplace(id, std::unique_ptr<Pipeline>(new Pipeline(m_socketName, id)));

    auto& pipeline = m_map[id];

    // unlock mapMutex for registerPipelineConnection may access map
    lock.unlock();
    auto status = pipeline->create(std::move(launch), std::move(config));
    lock.lock();

    if (status != PipelineStatus::SUCCESS)
        m_map.erase(id);

    return status;
}

PipelineStatus PipelineManager::deletePipeline(int id)
{
    std::lock_guard<std::mutex> lock(m_mapMutex);

    if (m_map.find(id) == m_map.end())
        return PipelineStatus::NOT_EXIST;
    // ignore return code since it will be removed
    m_map[id]->destroy();
    m_map.erase(id);

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineManager::modifyPipeline(int id, std::string config)
{
    std::lock_guard<std::mutex> lock(m_mapMutex);

    if (m_map.find(id) == m_map.end())
        return PipelineStatus::NOT_EXIST;
    auto status = m_map[id]->modify(std::move(config));

    cleanupPipeline(id, status);

    return status;
}

PipelineStatus PipelineManager::playPipeline(int id)
{
    std::lock_guard<std::mutex> lock(m_mapMutex);

    if (m_map.find(id) == m_map.end())
        return PipelineStatus::NOT_EXIST;
    auto status = m_map[id]->play();

    cleanupPipeline(id, status);

    return status;
}

PipelineStatus PipelineManager::stopPipeline(int id)
{
    std::lock_guard<std::mutex> lock(m_mapMutex);

    if (m_map.find(id) == m_map.end())
        return PipelineStatus::NOT_EXIST;
    auto status = m_map[id]->stop();

    cleanupPipeline(id, status);

    return status;
}

PipelineStatus PipelineManager::pausePipeline(int id)
{
    std::lock_guard<std::mutex> lock(m_mapMutex);

    if (m_map.find(id) == m_map.end())
        return PipelineStatus::NOT_EXIST;
    auto status = m_map[id]->pause();

    cleanupPipeline(id, status);

    return status;
}

void PipelineManager::cleanupPipeline(int id, PipelineStatus status)
{
    if (status == PipelineStatus::NOT_EXIST)
        m_map.erase(id);
}

void PipelineManager::createSocket(int socketId)
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

void PipelineManager::accept()
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

void PipelineManager::registerPipelineConnection()
{
    std::lock_guard<std::mutex> lock(m_socketMutex);

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
        if (m_map.find(id) != m_map.end())
            m_map[id]->establishConnection(std::move(m_tempSocket));
    }
}

}
