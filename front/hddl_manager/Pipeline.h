#ifndef _PIPELINE_H_
#define _PIPELINE_H_

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

#include "PipelineState.h"
#include "PipelineStatus.h"

namespace hddl {

class PipelineManager;

class Pipeline {
public:
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

    void sendEventToHost(PipelineEvent event);

    void setState(MPState state);
    MPState getState();

private:
    PipelineStatus mapStatus(std::pair<ReqType, MPState> map);

    int m_id;
    std::mutex m_mutex;
    MPState m_state;
    PipelineManager* m_manager;

    class Impl;
    std::unique_ptr<Impl> m_impl;
};
}

#endif // _PIPELINE_H_
