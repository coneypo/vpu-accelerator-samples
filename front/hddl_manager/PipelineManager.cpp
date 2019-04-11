#include "PipelineManager.h"

namespace hddl {

void PipelineManager::init(int socketId)
{
#ifndef MULTI_THREAD_MODE
    m_ipc.init(socketId);
#endif
}

void PipelineManager::uninit()
{
#ifndef MULTI_THREAD_MODE
    m_ipc.uninit();
#endif
    removeAll();
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

    m_map.emplace(id, std::unique_ptr<Pipeline>(new Pipeline(id)));

    auto pipeline = m_map[id];

    lock.unlock();
    auto status = pipeline->create(std::move(launch), std::move(config));
    lock.lock();

    if (status != PipelineStatus::SUCCESS)
        m_map.erase(id);

    return status;
}

PipelineStatus PipelineManager::deletePipeline(int id)
{
    auto pipeline = getPipeline(id);
    if (!pipeline)
        return PipelineStatus::NOT_EXIST;

    // ignore return code since it will be removed
    pipeline->destroy();

    cleanupPipeline(id, PipelineStatus::NOT_EXIST);

    return PipelineStatus::SUCCESS;
}

PipelineStatus PipelineManager::modifyPipeline(int id, std::string config)
{
    auto pipeline = getPipeline(id);
    if (!pipeline)
        return PipelineStatus::NOT_EXIST;

    auto status = pipeline->modify(std::move(config));

    cleanupPipeline(id, status);

    return status;
}

PipelineStatus PipelineManager::playPipeline(int id)
{
    auto pipeline = getPipeline(id);
    if (!pipeline)
        return PipelineStatus::NOT_EXIST;

    auto status = pipeline->play();

    cleanupPipeline(id, status);

    return status;
}

PipelineStatus PipelineManager::stopPipeline(int id)
{
    auto pipeline = getPipeline(id);
    if (!pipeline)
        return PipelineStatus::NOT_EXIST;

    auto status = pipeline->stop();

    cleanupPipeline(id, status);

    return status;
}

PipelineStatus PipelineManager::pausePipeline(int id)
{
    auto pipeline = getPipeline(id);
    if (!pipeline)
        return PipelineStatus::NOT_EXIST;

    auto status = pipeline->pause();

    cleanupPipeline(id, status);

    return status;
}

std::shared_ptr<Pipeline> PipelineManager::getPipeline(int id)
{
    std::lock_guard<std::mutex> lock(m_mapMutex);

    if (m_map.find(id) == m_map.end())
        return nullptr;

    return m_map[id];
}

void PipelineManager::cleanupPipeline(int id, PipelineStatus status)
{
    std::lock_guard<std::mutex> lock(m_mapMutex);

    if (status == PipelineStatus::NOT_EXIST)
        m_map.erase(id);
}
}
