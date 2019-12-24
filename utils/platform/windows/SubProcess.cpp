//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <string>
#include <unordered_set>
#include <vector>
#include <windows.h>

#include "utils/HLog.h"
#include "utils/SubProcess.h"

namespace HddlUnite {
class SubProcess::Impl {
public:
    Impl()
        : m_hide(true)
        , m_monitorEnabled(false)
        , m_argv(std::move(argv))
    {
        ZeroMemory(&m_processInfo, sizeof(m_processInfo));
    }

    ~Impl()
    {
        terminate();

        if (m_processInfo.hProcess) {
            CloseHandle(m_processInfo.hProcess);
            m_processInfo.hProcess = NULL;
        }

        if (m_processInfo.hThread) {
            CloseHandle(m_processInfo.hThread);
            m_processInfo.hThread = NULL;
        }
    }

    bool execute(std::string cmdline, bool hide = true)
    {
        STARTUPINFO si = { 0 };
        DWORD flags = hide ? 0 : CREATE_NEW_CONSOLE;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);

        if (!CreateProcess(NULL, // No module name (use command line)
                (LPTSTR)cmdline.data(), // Command line
                NULL, // Process handle not inheritable
                NULL, // Thread handle not inheritable
                FALSE, // Set handle inheritance to FALSE
                flags, // Share same console with parent process,"0"means sharing
                NULL, // Use parent's environment block
                NULL, // Use parent's starting directory
                &si, // Pointer to STARTUPINFO structure
                &m_processInfo) // Pointer to PROCESS_INFORMATION structure
        ) {
            HError("Error: CreateProcess failed with error(%d)", GetLastError());
            return false;
        }

        m_hide = hide;

        return true;
    }

    bool execute(bool hide = true)
    {
        if (m_argv.empty()) {
            HError("Error: launch subProcess failed: empty argument vector");
            return false;
        }

        std::stringstream ss;

        ss << '\"' << m_argv[0] << '\"'; // Double quote surrounding in case of any spaces within the executable path

        for (auto& arg : m_argv) {
            ss << " " << arg;
        }

        std::string cmdline = ss.str();

        return execute(cmdline, hide);
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
            std::thread monitor(&SubProcess::mointorRoutine, this);
            monitor.detach();
        }

        return true;
    }

    bool poll()
    {
        DWORD exitCode = 0;

        if (!m_processInfo.hProcess) {
            HError("Error: invalid process handle");
            return false;
        }

        if (!GetExitCodeProcess(m_processInfo.hProcess, &exitCode)) {
            HError("Error: GetExitCodeProcess failed with errorCode(%d)", GetLastError());
            return false;
        }

        return (exitCode == STILL_ACTIVE);
    }

    void terminate()
    {
        if (!poll()) {
            HError("Error: poll() failed");
            return;
        }

        // Terminate Process
        if (!TerminateProcess(m_processInfo.hProcess, 0x10071016)) {
            HError("Error: TerminateProcess failed with errorCode(%d)", GetLastError());
        }

        // Wait until child process exits.
        if (WaitForSingleObject(m_processInfo.hProcess, INFINITE) == WAIT_FAILED) {
            HError("Error: WaitForSingleObject failed with errorCode(%d)", GetLastError());
        }

        // Close process and thread handles.
        if (CloseHandle(m_processInfo.hProcess)) {
            m_processInfo.hProcess = NULL;
        } else {
            HError("Error: close process handle failed with errorCode(%d)", GetLastError());
        }

        if (CloseHandle(m_processInfo.hThread)) {
            m_processInfo.hProcess = NULL;
        } else {
            HError("Error: close thread handle failed with errorCode(%d)", GetLastError());
        }
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
        auto argv = m_argv;
        if (argv.back() != "--recover") {
            argv.emplace_back("--recover");
        }

        std::stringstream ss;

        for (auto& arg : argv) {
            ss << arg << ' ';
        }

        std::string command = ss.str();

        bool init = true;
        while (m_monitorEnabled) {
            DWORD retCode = WaitForSingleObject(m_processInfo.hProcess, INFINITE);
            if (retCode != WAIT_OBJECT_0 || !m_processInfo.hProcess) {
                HError("Error: WaitForSingleObject failed with retCode(%d)", retCode);
                break;
            }

            DWORD exitCode = 0;
            if (!GetExitCodeProcess(m_processInfo.hProcess, &exitCode)) {
                HError("Error: GetExitCodeProcess failed with errorCode(%d)", GetLastError());
            }

            if (exitCode == 0x10071016) {
                HDebug("Killed by daemon");
                break;
            }

            if (m_exitCodes.count(exitCode)) {
                if (!init) {
                    raise(SUGINT);
                }
                break;
            }

            if (m_processInfo.hProcess) {
                CloseHandle(m_processInfo.hProcess);
                m_processInfo.hProcess = nullptr;
            }

            if (m_processInfo.hThread) {
                CloseHandle(m_processInfo.hThread);
                m_processInfo.hThread = nullptr;
            }

            if (m_monitorEnabled) {
                execute(command.c_str(), m_hide);
            }

            init = false;
        }

        m_monitorEnabled = false;

        HDebug("Exit the daemon thread\n");
    }

private:
    bool m_hide;
    bool m_monitorEnabled;
    PROCESS_INFORMATION m_processInfo;
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
