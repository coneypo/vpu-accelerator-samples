#include "Pipeline.h"
#include <arpa/inet.h>
#include <chrono>
#include <thread>

namespace hddl {

#define TOKEN(x) #x
#define PREFIX(x) TOKEN(x)
#define MP_PATH PREFIX(INSTALL_DIR) "/bin/hddl_mediapipe3"

#define ERROR_RET(STATUS)                   \
    {                                       \
        auto ret = (STATUS);                \
        if (ret != PipelineStatus::SUCCESS) \
            return ret;                     \
    }

Pipeline::Pipeline(int pipeId)
    : m_id(pipeId)
    , m_state(MPState::NONEXIST)
{
    auto socketName = m_ipc.getSocketName();
    std::vector<std::string> argv = {
        MP_PATH,
        "-u",
        socketName,
        "-i",
        std::to_string(m_id)
    };
    m_proc = std::unique_ptr<SubProcess>(new SubProcess(argv));
}

Pipeline::~Pipeline()
{
    if (m_state != MPState::NONEXIST && m_proc->poll())
        m_proc->terminate();
    m_proc.reset();
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
    default:
        return PipelineStatus::ERROR;
    }
}

PipelineStatus Pipeline::isInStates(Pipeline::StateSet allowedStates, PipelineStatus defaultErrorStatus)
{
    auto state = m_state;
    if (allowedStates.find(state) != allowedStates.end())
        return PipelineStatus::SUCCESS;
    if (state != MPState::NONEXIST && defaultErrorStatus != PipelineStatus::SUCCESS)
        return defaultErrorStatus;
    return stateToStatus(state);
}

PipelineStatus Pipeline::isNotInStates(Pipeline::StateSet notAllowedStates, PipelineStatus defaultErrorStatus)
{
    auto state = m_state;
    if (notAllowedStates.find(state) == notAllowedStates.end())
        return PipelineStatus::SUCCESS;
    if (state != MPState::NONEXIST && defaultErrorStatus != PipelineStatus::SUCCESS)
        return defaultErrorStatus;
    return stateToStatus(state);
}

PipelineStatus Pipeline::create(std::string launch, std::string config)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ERROR_RET(isInStates({ MPState::NONEXIST }, PipelineStatus::ALREADY_CREATED));

    if (!m_proc->execute())
        return PipelineStatus::INVALID_PARAMETER;

    m_proc->enableWaitNotify([this] {
        std::lock_guard<std::mutex> l(m_mutex);
        m_state = MPState::NONEXIST;
    });

    auto sts = m_ipc.create(m_id, std::move(launch), std::move(config));
    if (sts != PipelineStatus::SUCCESS)
        return sts;

    m_state = MPState::CREATED;

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::modify(std::string config)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ERROR_RET(isNotInStates({ MPState::NONEXIST, MPState::STOPPED }));

    return m_ipc.modify(m_id, std::move(config));
}

PipelineStatus Pipeline::destroy()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ERROR_RET(isNotInStates({ MPState::NONEXIST }, PipelineStatus::NOT_EXIST));

    auto sts = m_ipc.destroy(m_id);
    if (sts != PipelineStatus::SUCCESS)
        return sts;

    m_state = MPState::NONEXIST;

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::play()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ERROR_RET(isInStates({ MPState::CREATED, MPState::PAUSED }));

    auto sts = m_ipc.play(m_id);
    if (sts != PipelineStatus::SUCCESS)
        return sts;

    m_state = MPState::PLAYING;

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::stop()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ERROR_RET(isInStates({ MPState::PLAYING, MPState::PAUSED }, PipelineStatus::NOT_PLAYING));

    auto sts = m_ipc.stop(m_id);
    if (sts != PipelineStatus::SUCCESS)
        return sts;

    m_state = MPState::STOPPED;

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::pause()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ERROR_RET(isInStates({ MPState::PLAYING }, PipelineStatus::NOT_PLAYING));

    auto sts = m_ipc.pause(m_id);
    if (sts != PipelineStatus::SUCCESS)
        return sts;

    m_state = MPState::PAUSED;

    return PipelineStatus::SUCCESS;
}

}
