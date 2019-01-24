#ifndef _PIPELINE_H_
#define _PIPELINE_H_

#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "SubProcess.h"
#include <hddl_message.pb.h>

namespace hddl {

enum class PipelineStatus : int {
    SUCCESS = 0,
    ERROR = -1,
    COMM_TIMEOUT = -2,
    INVALID_PARAMETER = -3,
    NOT_EXIST = -4,
    ALREADY_CREATED = -5,
    ALREADY_STARTED = -6,
    NOT_PLAYING = -7,
    STOPPED = -8
};

class Pipeline {
public:
    using Socket = boost::asio::local::stream_protocol::socket;

    Pipeline(int id);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    PipelineStatus create(std::string launch, std::string config);
    PipelineStatus modify(std::string config);
    PipelineStatus destroy();
    PipelineStatus play();
    PipelineStatus stop();
    PipelineStatus pause();

    /*
     * The PipelineManager should call this to set connection socket back.
     * In create there's timeout waiting on this.
     */
    void establishConnection(std::unique_ptr<Socket> socket)
    {
        m_socket = std::move(socket);
    }

private:
    enum class MPState : int {
        NONEXIST,
        CREATED,
        PLAYING,
        PAUSED,
        STOPPED
    };

    // Using enum class in set/map directly is not allowed in C++11, this 'Hash' won't be necessary in C++14.
    struct Hash {
        template <typename T>
        std::size_t operator()(T t) const
        {
            return static_cast<std::size_t>(t);
        }
    };
    using StateSet = std::unordered_set<MPState, Hash>;

    /*
     * Check state whether in allowed/not_allowed states.
     * When yes:
     *     return SUCCESS.
     * When not:
     *     if defaultErrorStatus is not SUCCESS, return defaultErrorStatus;
     *     otherwise return corresponding status.
     */
    PipelineStatus isInStates(StateSet allowedStates, PipelineStatus defaultErrorStatus = PipelineStatus::SUCCESS);
    PipelineStatus isNotInStates(StateSet notAllowedStates, PipelineStatus defaultErrorStatus = PipelineStatus::SUCCESS);

    PipelineStatus stateToStatus(MPState state);

    MsgRequest createRequest(MsgReqType type);
    std::unique_ptr<MsgResponse> sendRequest(const MsgRequest& request);

    int m_id;
    unsigned long m_seq_no;
    std::unique_ptr<SubProcess> m_proc;

    std::mutex m_mutex;
    MPState m_state;

    std::unique_ptr<Socket> m_socket;
};

}

#endif // _PIPELINE_H_
