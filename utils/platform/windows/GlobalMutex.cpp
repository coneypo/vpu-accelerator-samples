//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <cstring>
#include <windows.h>

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

    /* create a mutex with no initial owner */
    m_globalMutex = CreateMutex(nullptr, false, m_name.c_str());

    if (!m_globalMutex) {
        HError("Error: Create GlobalMutex %s failed. errno = %d [%s]", m_name, errno, strerror(errno));
    }
}

GlobalMutex::~GlobalMutex()
{
    if (m_globalMutex) {
        CloseHandle(m_globalMutex);
        m_globalMutex = nullptr;
    }
}

bool GlobalMutex::lock()
{
    AutoMutex autoLock(m_mutex);

    /* If the function succeeds, the return value indicates the event that caused the function to return. */
    DWORD ret = WaitForSingleObject(m_globalMutex, INFINITE);

    if (ret != WAIT_OBJECT_0) {
        HError("Error: Lock GlobalMutex(%s) failed, errno = %d [%s]", m_name, errno, strerror(errno));
        if (ret == WAIT_ABANDONED) {
            HError("Error: The mutex owner crashed, release the mutex");
            ReleaseMutex(m_globalMutex);
        }
        return false;
    }

    return true;
}

bool GlobalMutex::lock(long timeoutMilliseconds)
{
    AutoMutex autoLock(m_mutex);

    DWORD ret = WaitForSingleObject(m_globalMutex, timeoutMilliseconds);

    return (ret == WAIT_OBJECT_0);
}

bool GlobalMutex::trylock()
{
    AutoMutex autoLock(m_mutex);

    /* If the function succeeds, the return value indicates the event that caused the function to return. */
    DWORD ret = WaitForSingleObject(m_globalMutex, 0);

    return (ret == WAIT_OBJECT_0);
}

void GlobalMutex::unlock()
{
    AutoMutex autoLock(m_mutex);

    /* If the function succeeds, the return value is nonzero */
    ReleaseMutex(m_globalMutex);
}
}
