#ifndef _PIPELINE_H_
#define _PIPELINE_H_

#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

#include "PipelineStatus.h"

namespace hddl {

class Pipeline {
public:
    enum class MPState : int {
        NONEXIST,
        CREATED,
        PLAYING,
        PAUSED,
        STOPPED,
        RUNTIME_ERROR,
        PIPELINE_EOS
    };

    Pipeline(int pipeId);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    PipelineStatus create(std::string launch, std::string config);
    PipelineStatus modify(std::string config);
    PipelineStatus destroy();
    PipelineStatus play();
    PipelineStatus stop();
    PipelineStatus pause();

    void setState(MPState state);

private:
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

    int m_id;
    std::mutex m_mutex;
    MPState m_state;

    class Impl;
    std::unique_ptr<Impl> m_impl;
};
}

#endif // _PIPELINE_H_
