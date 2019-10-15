/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "Pipeline.h"
#include "PipelineIPC.h"
#include "PipelineIpcClient.h"
#include "SubProcess.h"

namespace hddl {

#define TOKEN(x) #x
#define PREFIX(x) TOKEN(x)
#define MP_PATH         \
    PREFIX(INSTALL_DIR) \
    "/bin/hddl_mediapipe3"

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
        m_ipc.cleanupIpcClient(m_pipe.m_id);
    }

    PipelineStatus create(std::string launch, std::string config)
    {
        if (!m_proc->execute())
            return PipelineStatus::INVALID_PARAMETER;

        m_proc->enableWaitNotify([this] {
            std::lock_guard<std::mutex> l(m_pipe.m_mutex);
            m_pipe.m_state = MPState::NONEXIST;
        });

        m_ipcClient = m_ipc.getIpcClient(m_pipe.m_id, 10);
        m_ipcClient->setIpcClient(&m_pipe);

        if (!m_ipcClient)
            return PipelineStatus::COMM_TIMEOUT;

        m_ipcClient->readResponse();

        auto sts = m_ipcClient->create(std::move(launch), std::move(config));
        if (sts != PipelineStatus::SUCCESS)
            return sts;

        return PipelineStatus::SUCCESS;
    }

    PipelineStatus modify(std::string config)
    {
        return m_ipcClient->modify(std::move(config));
    }

    PipelineStatus destroy()
    {
        auto sts = m_ipcClient->destroy();
        if (sts != PipelineStatus::SUCCESS)
            return sts;

        return PipelineStatus::SUCCESS;
    }

    PipelineStatus play()
    {
        auto sts = m_ipcClient->play();
        if (sts != PipelineStatus::SUCCESS)
            return sts;

        return PipelineStatus::SUCCESS;
    }

    PipelineStatus stop()
    {
        auto sts = m_ipcClient->stop();
        if (sts != PipelineStatus::SUCCESS)
            return sts;

        return PipelineStatus::SUCCESS;
    }

    PipelineStatus pause()
    {
        auto sts = m_ipcClient->pause();
        if (sts != PipelineStatus::SUCCESS)
            return sts;

        return PipelineStatus::SUCCESS;
    }

    PipelineStatus setChannel(const std::string& element, const int channelId)
    {
        return m_ipcClient->setChannel(element, channelId);
    }

private:
    std::unique_ptr<SubProcess> m_proc;
    PipelineIPC& m_ipc = PipelineIPC::getInstance();
    PipelineIpcClient::Ptr m_ipcClient;
    Pipeline& m_pipe;
};
}
