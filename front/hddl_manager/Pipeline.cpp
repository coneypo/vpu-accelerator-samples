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
    , m_impl(new Impl(this))
{
}

Pipeline::~Pipeline()
{
}

PipelineStatus Pipeline::stateToStatus(Pipeline::MPState state)
{
    switch (state) {
    case MPState::NONEXIST:
        return PipelineStatus::NOT_EXIST;
    case MPState::CREATED:
        return PipelineStatus::ALREADY_CREATED;
    case MPState::PLAYING:
        return PipelineStatus::ALREADY_STARTED;
    case MPState::PAUSED:
        return PipelineStatus::NOT_PLAYING;
    case MPState::STOPPED:
        return PipelineStatus::STOPPED;
    case MPState::PIPELINE_EOS:
        return PipelineStatus::PIPELINE_EOS;
    case MPState::RUNTIME_ERROR:
        return PipelineStatus::RUNTIME_ERROR;
    default:
        return PipelineStatus::ERROR;
    }
}

PipelineStatus Pipeline::isInStates(Pipeline::StateSet allowedStates, PipelineStatus defaultErrorStatus)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto state = m_state;
    if (allowedStates.find(state) != allowedStates.end())
        return PipelineStatus::SUCCESS;
    if (state != MPState::NONEXIST && state != MPState::PIPELINE_EOS && state != MPState::RUNTIME_ERROR && defaultErrorStatus != PipelineStatus::SUCCESS)
        return defaultErrorStatus;
    return stateToStatus(state);
}

PipelineStatus Pipeline::isNotInStates(Pipeline::StateSet notAllowedStates, PipelineStatus defaultErrorStatus)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto state = m_state;
    if (notAllowedStates.find(state) == notAllowedStates.end())
        return PipelineStatus::SUCCESS;
    if (state != MPState::NONEXIST && state != MPState::PIPELINE_EOS && state != MPState::RUNTIME_ERROR && defaultErrorStatus != PipelineStatus::SUCCESS)
        return defaultErrorStatus;
    return stateToStatus(state);
}

void Pipeline::setState(MPState state)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = state;
}

PipelineStatus Pipeline::create(std::string launch, std::string config)
{
    ERROR_RET(isInStates({ MPState::NONEXIST }, PipelineStatus::ALREADY_CREATED));

    auto sts = m_impl->create(std::move(launch), std::move(config));
    if (sts != PipelineStatus::SUCCESS)
        return sts;

    setState(MPState::CREATED);

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::modify(std::string config)
{
    ERROR_RET(isNotInStates({ MPState::NONEXIST, MPState::STOPPED , MPState::PIPELINE_EOS, MPState::RUNTIME_ERROR }));

    return m_impl->modify(std::move(config));
}

PipelineStatus Pipeline::destroy()
{
    ERROR_RET(isNotInStates({ MPState::NONEXIST }, PipelineStatus::NOT_EXIST));

    auto sts = m_impl->destroy();
    if (sts != PipelineStatus::SUCCESS)
        return sts;

    setState(MPState::NONEXIST);

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::play()
{
    ERROR_RET(isInStates({ MPState::CREATED, MPState::PAUSED }));

    auto sts = m_impl->play();
    if (sts != PipelineStatus::SUCCESS)
        return sts;

    setState(MPState::PLAYING);

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::stop()
{
    ERROR_RET(isInStates({ MPState::PLAYING, MPState::PAUSED, MPState::RUNTIME_ERROR, MPState::PIPELINE_EOS }, PipelineStatus::NOT_PLAYING));

    auto tmp = m_state;

    auto sts = m_impl->stop();
    if (sts < PipelineStatus::SUCCESS)
        return sts;

    setState(MPState::STOPPED);

    if (tmp == MPState::PIPELINE_EOS || tmp == MPState::RUNTIME_ERROR)
        return stateToStatus(tmp);
    else
        return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::pause()
{
    ERROR_RET(isInStates({ MPState::PLAYING }, PipelineStatus::NOT_PLAYING));

    auto sts = m_impl->pause();
    if (sts != PipelineStatus::SUCCESS)
        return sts;

    setState(MPState::PAUSED);

    return PipelineStatus::SUCCESS;
}

}
