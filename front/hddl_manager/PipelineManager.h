#ifndef _PIPELINEMANAGER_H_
#define _PIPELINEMANAGER_H_

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Pipeline.h"
#include "PipelineStatus.h"
#ifndef MANAGER_THREAD
#include "PipelineIPC.h"
#endif

namespace hddl {

class PipelineManager {
public:
    PipelineManager(const PipelineManager&) = delete;
    PipelineManager& operator=(const PipelineManager&) = delete;

    void init(int socketId = 0);
    void uninit();

    PipelineStatus removeAll();
    std::vector<int> getAll();

    PipelineStatus addPipeline(int id, std::string launch, std::string config);
    PipelineStatus deletePipeline(int id);
    PipelineStatus modifyPipeline(int id, std::string config);
    PipelineStatus playPipeline(int id);
    PipelineStatus stopPipeline(int id);
    PipelineStatus pausePipeline(int id);

    static PipelineManager& getInstance()
    {
        static PipelineManager instance;
        return instance;
    }

private:
    PipelineManager() = default;
    ~PipelineManager() = default;

    std::shared_ptr<Pipeline> getPipeline(int id);
    void cleanupPipeline(int id, PipelineStatus status);

    using Map = std::unordered_map<int, std::shared_ptr<Pipeline>>;
    std::mutex m_mapMutex;
    Map m_map;

#ifndef MANAGER_THREAD
    PipelineIPC& m_ipc = PipelineIPC::getInstance();
#endif
};

}

#endif // _PIPELINEMANAGER_H_
