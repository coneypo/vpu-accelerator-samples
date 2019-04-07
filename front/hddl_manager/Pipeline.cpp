#include "Pipeline.h"

#ifdef MANAGER_THREAD
#include "PipelineThread.cpp"
#else
#include "PipelineProcess.cpp"
#endif

namespace hddl {

#define ERROR_RET(STATUS)                   \
    {                                       \
        auto ret = (STATUS);                \
        if (ret != PipelineStatus::SUCCESS) \
            return ret;                     \
    }

Pipeline::Pipeline(int pipeId)
    : m_id(pipeId)
    , m_state(MPState::NONEXIST)
    , m_map(constStateMap)
    , m_impl(new Impl(this))
{
}

Pipeline::~Pipeline()
{
}

PipelineStatus Pipeline::mapStatus(std::pair<ReqType, MPState> map)
{
    auto ret = m_map.find(map);

    if (ret == m_map.end()) {
        return PipelineStatus::ERROR;
    }

    return ret->second;
}

void Pipeline::setState(MPState state)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = state;
}

PipelineStatus Pipeline::create(std::string launch, std::string config)
{
    ERROR_RET(mapStatus({ ReqType::CREATE, m_state }));

    auto sts = m_impl->create(std::move(launch), std::move(config));
    if (sts != PipelineStatus::SUCCESS)
        return sts;

    setState(MPState::CREATED);

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::modify(std::string config)
{
    ERROR_RET(mapStatus({ ReqType::MODIFY, m_state }));

    return m_impl->modify(std::move(config));
}

PipelineStatus Pipeline::destroy()
{
    ERROR_RET(mapStatus({ ReqType::DESTROY, m_state }));

    auto sts = m_impl->destroy();
    if (sts != PipelineStatus::SUCCESS)
        return sts;

    setState(MPState::NONEXIST);

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::play()
{
    ERROR_RET(mapStatus({ ReqType::PLAY, m_state }));

    auto sts = m_impl->play();
    if (sts != PipelineStatus::SUCCESS)
        return sts;

    setState(MPState::PLAYING);

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::stop()
{
    ERROR_RET(mapStatus({ ReqType::STOP, m_state }));

    auto tmp = m_state;

    auto sts = m_impl->stop();
    if (sts < PipelineStatus::SUCCESS)
        return sts;

    setState(MPState::STOPPED);

    if (tmp == MPState::PIPELINE_EOS)
        return PipelineStatus::PIPELINE_EOS;
    else if (tmp == MPState::RUNTIME_ERROR)
        return PipelineStatus::RUNTIME_ERROR;

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::pause()
{
    ERROR_RET(mapStatus({ ReqType::PAUSE, m_state }));

    auto sts = m_impl->pause();
    if (sts != PipelineStatus::SUCCESS)
        return sts;

    setState(MPState::PAUSED);

    return PipelineStatus::SUCCESS;
}
}
