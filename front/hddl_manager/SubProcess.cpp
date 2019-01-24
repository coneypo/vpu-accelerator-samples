#include <thread>

#include <algorithm>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "SubProcess.h"

SubProcess::SubProcess(std::vector<std::string> argv)
    : m_processId(-1)
    , m_waitEnabled(false)
    , m_argv(std::move(argv))
{
}

SubProcess::~SubProcess()
{
    terminate();

    // wait for process exiting callback finished
    std::unique_lock<std::mutex> lock(m_waitMutex);
    m_waitCond.wait(lock, [this] { return !m_waitEnabled; });
}

bool SubProcess::execute()
{
    if (m_argv.empty()) {
        return false;
    }

    auto pid = fork();
    if (pid < 0) {
        return false;
    } else if (pid == 0) {
        char** argVec = new char*[m_argv.size() + 1];
        int i = 0;
        for (auto& arg : m_argv)
            argVec[i++] = const_cast<char*>(arg.c_str());
        argVec[i] = NULL;

        execv(argVec[0], argVec);
        _exit(EXIT_FAILURE);
    }

    m_processId = pid;

    return true;
}

bool SubProcess::enableWaitNotify(std::function<void()> callback)
{
    if (m_waitEnabled)
        return false;

    if (!poll())
        return false;

    std::thread monitor(&SubProcess::waitRoutine, this, callback);
    monitor.detach();

    return true;
}

bool SubProcess::poll()
{
    if (m_processId < 0)
        return false;

    if (kill(m_processId, 0)) {
        if (errno == ESRCH)
            return false;
    }

    return true;
}

void SubProcess::terminate()
{
    if (!poll())
        return;

    kill(m_processId, SIGTERM);

    m_processId = -1;
}

void SubProcess::waitRoutine(std::function<void()> callback)
{
    m_waitEnabled = true;

    int status = 0;
    if (waitpid(m_processId, &status, 0) == -1)
        return;

    if (callback)
        callback();

    {
        std::lock_guard<std::mutex> lock(m_waitMutex);
        m_waitEnabled = false;
    }
    m_waitCond.notify_one();
}
