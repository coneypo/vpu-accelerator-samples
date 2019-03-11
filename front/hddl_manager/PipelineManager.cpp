#include "PipelineManager.h"

namespace hddl {

void PipelineManager::init(int socketId)
{
    m_ipc.init(socketId);
}

void PipelineManager::uninit()
{
    m_ipc.uninit();
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
    std::lock_guard<std::mutex> lock(m_mapMutex);

    if (m_map.find(id) != m_map.end())
        return PipelineStatus::ALREADY_CREATED;

    m_map.emplace(id, std::unique_ptr<Pipeline>(new Pipeline(id)));

    auto status = m_map[id]->create(std::move(launch), std::move(config));

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

}
