//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <iostream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <cerrno>
#include <csignal>
#include <dirent.h>
#include <sys/wait.h>

#include "utils/HLog.h"
#include "utils/SubProcess.h"

namespace HddlUnite {
class SubProcess::Impl {
public:
    explicit Impl(std::vector<std::string> argv)
        : m_processId(-1)
        , m_monitorEnabled(false)
        , m_argv(std::move(argv))
    {
    }

    ~Impl()
    {
        terminate();
    }

    bool execute(bool hide = true)
    {
        if (m_argv.empty()) {
            HError("Error: argument vector should not be empty.");
            return false;
        }

        pid_t pid = fork();

        if (pid > 0) {
            m_processId = pid;
            return true;
        } else if (pid < 0) {
            HError("Error: fork() failed, error=%s", strerror(errno));
            return false;
        }

        size_t argc = m_argv.size();
        std::vector<char*> argList(argc + 1, nullptr);

        for (size_t i = 0; i < argc; i++) {
            argList[i] = const_cast<char*>(m_argv[i].c_str());
        }

        UNUSED(hide);

        execv(argList[0], &argList[0]);
        HError("Error: process(%d) exits with failure, error=%s", pid, strerror(errno));

        _exit(EXIT_FAILURE);
    }

    bool enableDaemon(bool enabled)
    {
        if (m_monitorEnabled == enabled) {
            return true;
        }

        if (!poll()) {
            return false;
        }

        m_monitorEnabled = enabled;

        if (m_monitorEnabled) {
            std::thread monitor(&Impl::mointorRoutine, this);
            monitor.detach();
        }

        return true;
    }

    bool poll()
    {
        if (m_processId < 0) {
            return false;
        }

        // Kill 0
        if (kill(m_processId, 0)) {
            if (errno == ESRCH) {
                return false;
            }
        }

        return true;
    }

    void terminate()
    {
        if (!poll()) {
            HDebug("Poll Failed.");
            return;
        }

        // Terminate Process kill -9
        kill(m_processId, SIGTERM);

        m_processId = -1;
    }

    void addExitCode(int exitCode)
    {
        m_exitCodes.insert(exitCode);
    }

    void addExtraOption(std::string option)
    {
        m_extraOptions.push_back(std::move(option));
    }

    void setExecuteCommand(std::vector<std::string> argv)
    {
        m_argv = std::move(argv);
    }

    void mointorRoutine()
    {
        /*
         * This workaround is for Linux driver myd_vsc, which creates /dev/myriadX device files. These device
         * files should be opened exclusively for the process using devices if the driver code doesn't change.
         * While hddldaemon opens them, after that any child process inherits FDs. When the child tries to close
         * them (no matter FD_CLOEXEC or exit or close), it will cause problem since the driver char device do
         * some cleanup at close/flush.
         *
         * This disables file descriptors sharing for the current thread with other threads of hddldaemon, this should
         * be run in a thread and started before any ncOpenDeviceBooted() (so /dev/myriadX file is not opened). When
         * it starts new autoboot in the future, the file descriptor table will not include fd for /dev/myriadX, thus
         * this problem is avoided.
         */
        // unshare(CLONE_FILES); // fixed in latest linux driver myd_vsc

        bool init = true;
        while (m_monitorEnabled) {
            int status = 0;
            if (waitpid(m_processId, &status, 0) == -1) {
                HError("Failed to check subprocess status");
            }

            if (WIFEXITED(status) && m_exitCodes.count(WEXITSTATUS(status))) {
                if (!init) {
                    raise(SIGUSR1);
                }
                break;
            }

            for (const auto& option : m_extraOptions) {
                if (std::find(m_argv.begin(), m_argv.end(), option) == m_argv.end()) {
                    m_argv.push_back(option);
                }
            }

            if (m_monitorEnabled) {
                execute();
            }

            init = false;
        }

        HDebug("Exit the mointor thread");
    }

private:
    pid_t m_processId;
    bool m_monitorEnabled;
    std::vector<std::string> m_argv;
    std::unordered_set<int> m_exitCodes; // monitorRoutine() will exit if subPorcess's exitCode in m_exitCodes
    std::vector<std::string> m_extraOptions; // these options will be appended to m_argv if not found in m_argv
};

SubProcess::SubProcess(std::vector<std::string> argv)
    : impl(new Impl(std::move(argv)))
{
}

SubProcess::~SubProcess()
{
    impl.reset();
}

bool SubProcess::execute(bool hide)
{
    return impl->execute(hide);
}

bool SubProcess::enableDaemon(bool enabled)
{
    return impl->enableDaemon(enabled);
}

bool SubProcess::poll()
{
    return impl->poll();
}

void SubProcess::terminate()
{
    return impl->terminate();
}

void SubProcess::addExitCode(int exitCode)
{
    return impl->addExitCode(exitCode);
}

void SubProcess::addExtraOption(std::string option)
{
    return impl->addExtraOption(std::move(option));
}

void SubProcess::setExecuteCommand(std::vector<std::string> argv)
{
    return impl->setExecuteCommand(std::move(argv));
}
}
