#include "Pipeline.h"
#include "PipelineIPC.h"
#include "SubProcess.h"

namespace hddl {

#define TOKEN(x) #x
#define PREFIX(x) TOKEN(x)
#define MP_PATH PREFIX(INSTALL_DIR) "/bin/hddl_mediapipe3"

class Pipeline::Impl {
public:
    Impl(Pipeline* parent)
        : m_pipe(*parent)
    {
        auto socketName = m_ipc.getSocketName();
        std::vector<std::string> argv = {
                MP_PATH,
                "-u",
                socketName,
                "-i",
                std::to_string(m_pipe.m_id)
        };
        m_proc = std::unique_ptr<SubProcess>(new SubProcess(argv));
    }

    ~Impl()
    {
        if (m_proc->poll())
            m_proc->terminate();
        m_proc.reset();
    }

    PipelineStatus create(std::string launch, std::string config)
    {
        if (!m_proc->execute())
            return PipelineStatus::INVALID_PARAMETER;

        m_proc->enableWaitNotify([this] {
            std::lock_guard<std::mutex> l(m_pipe.m_mutex);
            m_pipe.m_state = MPState::NONEXIST;
        });

        auto sts = m_ipc.create(m_pipe.m_id, std::move(launch), std::move(config));
        if (sts != PipelineStatus::SUCCESS)
            return sts;

        return PipelineStatus::SUCCESS;
    }

    PipelineStatus modify(std::string config)
    {
        return m_ipc.modify(m_pipe.m_id, std::move(config));
    }

    PipelineStatus destroy()
    {
        auto sts = m_ipc.destroy(m_pipe.m_id);
        if (sts != PipelineStatus::SUCCESS)
            return sts;

        return PipelineStatus::SUCCESS;
    }

    PipelineStatus play()
    {
        auto sts = m_ipc.play(m_pipe.m_id);
        if (sts != PipelineStatus::SUCCESS)
            return sts;

        return PipelineStatus::SUCCESS;
    }

    PipelineStatus stop()
    {
        auto sts = m_ipc.stop(m_pipe.m_id);
        if (sts != PipelineStatus::SUCCESS)
            return sts;

        return PipelineStatus::SUCCESS;
    }

    PipelineStatus pause()
    {
        auto sts = m_ipc.pause(m_pipe.m_id);
        if (sts != PipelineStatus::SUCCESS)
            return sts;

        return PipelineStatus::SUCCESS;
    }

private:
    std::unique_ptr<SubProcess> m_proc;
    PipelineIPC& m_ipc = PipelineIPC::getInstance();
    Pipeline& m_pipe;
};

}
