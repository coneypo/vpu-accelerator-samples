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

Pipeline::Pipeline(int id)
    : m_id(id)
    , m_seq_no(0)
    , m_state(MPState::NONEXIST)
{
    std::vector<std::string> argv = {
        MP_PATH,
        "-u",
        "/tmp/hddl_manager.sock",
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

MsgRequest Pipeline::createRequest(MsgReqType type)
{
    MsgRequest request;
    request.set_req_type(type);
    request.set_pipeline_id(m_id);
    request.set_req_seq_no(m_seq_no++);

    return request;
}

std::unique_ptr<MsgResponse> Pipeline::sendRequest(const MsgRequest& request)
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

    int count = 100;
    while (!m_socket && count) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (--count <= 0) {
            m_proc->terminate();
            return PipelineStatus::COMM_TIMEOUT;
        }
    }

    auto request = createRequest(CREATE_REQUEST);
    request.mutable_create()->set_launch_data(launch);
    request.mutable_create()->set_config_data(config);

    auto response = sendRequest(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    m_state = MPState::CREATED;

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::modify(std::string config)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ERROR_RET(isNotInStates({ MPState::NONEXIST, MPState::STOPPED }));

    auto request = createRequest(MODIFY_REQUEST);
    request.mutable_modify()->set_config_data(config);

    auto response = sendRequest(request);
    if (!response)
        return PipelineStatus::ERROR;
    if (response->ret_code() != 0)
        return PipelineStatus::INVALID_PARAMETER;

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::destroy()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ERROR_RET(isNotInStates({ MPState::NONEXIST }, PipelineStatus::NOT_EXIST));

    auto request = createRequest(DESTROY_REQUEST);

    auto response = sendRequest(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    m_state = MPState::NONEXIST;

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::play()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ERROR_RET(isInStates({ MPState::CREATED, MPState::PAUSED }));

    auto request = createRequest(PLAY_REQUEST);

    auto response = sendRequest(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    m_state = MPState::PLAYING;

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::stop()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ERROR_RET(isInStates({ MPState::PLAYING, MPState::PAUSED }, PipelineStatus::NOT_PLAYING));

    auto request = createRequest(STOP_REQUEST);

    auto response = sendRequest(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    m_state = MPState::STOPPED;

    return PipelineStatus::SUCCESS;
}

PipelineStatus Pipeline::pause()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ERROR_RET(isInStates({ MPState::PLAYING }, PipelineStatus::NOT_PLAYING));

    auto request = createRequest(PAUSE_REQUEST);

    auto response = sendRequest(request);
    if (!response || response->ret_code() != 0)
        return PipelineStatus::ERROR;

    m_state = MPState::PAUSED;

    return PipelineStatus::SUCCESS;
}

}
