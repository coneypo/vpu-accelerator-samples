#ifndef _LOCALMODE_H
#define _LOCALMODE_H

#include <string>
#include <unordered_map>

#include "PipelineManager.h"

namespace hddl {
class LocalMode {
public:
    LocalMode(const LocalMode&) = delete;
    LocalMode& operator=(const LocalMode&) = delete;

    int init(std::string& localFile, PipelineManager& pipeMgr, int seconds);

    void uninit();
    void run();

    static LocalMode& getInstance()
    {
        static LocalMode instance;
        return instance;
    }

private:
    // struct to store pipeline's information
    struct Info {
        std::string launchData;
        std::string configData;
        int threads;
    };

    LocalMode() = default;
    ~LocalMode() = default;

    void launchStartPipelines();
    void stopDestroyPipelines();

    int readFile(std::string& localFile, std::string& localData);
    int parseJson(std::string& localData);

    PipelineManager* m_pipeManager;
    std::vector<Info> m_pipeInfo;
    std::unordered_map<int, bool> m_pipeLaunched;
    int m_pipeId = 0;
    int m_runSeconds = 10;
};
}

#endif // _LOCALMODE_H_
