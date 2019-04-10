#ifndef _PIPELINEMANAGER_H_
#define _PIPELINEMANAGER_H_

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Pipeline.h"
#include "PipelineStatus.h"
#ifndef MULTI_THREAD_MODE
#include "PipelineIPC.h"
#endif

namespace hddl {

class PipelineManager {
public:
    enum class LoadFileType : uint8_t {
        CREATE = 0,
        OVERWRITE = 1,
        APPEND = 2
    };

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

    PipelineStatus loadFile(const std::string& data, const std::string& dstPath, uint64_t fileMode, LoadFileType flag);
    PipelineStatus unloadFile(const std::string& filePath);

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
    bool exist(const char* path);
    bool exist(const std::string& path);
    bool changeFileMode(const char* file, int mode);

    using Map = std::unordered_map<int, std::shared_ptr<Pipeline>>;
    std::mutex m_mapMutex;
    Map m_map;

#ifndef MULTI_THREAD_MODE
    PipelineIPC& m_ipc = PipelineIPC::getInstance();
#endif
};
}

#endif // _PIPELINEMANAGER_H_
