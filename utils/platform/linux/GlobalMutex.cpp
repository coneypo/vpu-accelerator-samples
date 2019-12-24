//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <thread>
#include <unistd.h>

#include "utils/GlobalMutex.h"
#include "utils/HLog.h"

namespace HddlUnite {
GlobalMutex::GlobalMutex(std::string name, bool closeOnExec)
    : m_name(std::move(name))
    , m_fd(0)
    , m_globalMutex(nullptr)
{
    if (m_name.empty()) {
        return;
    }

    m_fd = open(m_name.c_str(), O_CREAT, 0660);

    if (m_fd < 0) {
        HError("Error: Open GlobalMutex %s failed. errno = %d [%s]", m_name, errno, strerror(errno));
        return;
    }

    if (closeOnExec) {
        int flags = fcntl(m_fd, F_GETFD) | FD_CLOEXEC;
        fcntl(m_fd, F_SETFD, flags);
    }
}

GlobalMutex::~GlobalMutex()
{
    if (m_fd > 0) {
        close(m_fd);
        m_fd = 0;
    }
}

bool GlobalMutex::lock()
{
    AutoMutex autoLock(m_mutex);

    if (m_fd <= 0) {
        HError("Error: GlobalMutex %s is not initialized.", m_name);
        return false;
    }

    if (flock(m_fd, LOCK_EX) < 0) {
        HError("Error: Lock GlobalMutex(%s) failed. errno = %d [%s]", m_name, errno, strerror(errno));
        return false;
    }

    return true;
}

bool GlobalMutex::lock(long timeoutMilliseconds)
{

    AutoMutex autoLock(m_mutex);

    // Because there is no timeout lock in flock in Linux,
    // this implementation is a workaround for this function.
    if (m_fd <= 0) {
        HError("Error: GlobalMutex %s is not initialized.", m_name);
        return false;
    }

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    while (flock(m_fd, LOCK_EX | LOCK_NB) < 0) {
        std::chrono::steady_clock::time_point n = std::chrono::steady_clock::now();
        std::chrono::milliseconds d = std::chrono::duration_cast<std::chrono::milliseconds>(n - now);

        if (d.count() >= timeoutMilliseconds) {
            /* Time out */
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return true;
}

bool GlobalMutex::trylock()
{
    AutoMutex autoLock(m_mutex);

    if (m_fd <= 0) {
        HError("Error: GlobalMutex %s is not initialized.", m_name);
        return false;
    }

    if (flock(m_fd, LOCK_EX | LOCK_NB) < 0) {
        return false;
    }

    return true;
}

void GlobalMutex::unlock()
{
    AutoMutex autoLock(m_mutex);

    flock(m_fd, LOCK_UN);
}
}
