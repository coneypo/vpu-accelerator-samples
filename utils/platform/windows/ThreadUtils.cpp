//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <intrin.h>
#include <winsock.h>

#include "ThreadUtils.h"
#include "utils/ThreadUtils.h"

namespace ThreadUtils {
const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push, 8)
typedef struct
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;

#pragma pack(pop)

static void setThreadName(DWORD dwThreadID, const char* threadName)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable : 6320 6322)
    __try {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
#pragma warning(pop)
}

DWORD getThreadId()
{
    return GetCurrentThreadId();
}

DWORD getProcessId()
{
    return GetCurrentProcessId();
}

void setName(const char* pname)
{
    setThreadName(GetCurrentThreadId(), pname);
}

void getName(char* name, size_t size)
{
}

#pragma intrinsic(__rdtsc)

uint64_t getRdtsc()
{
    return __rdtsc();
}

#pragma comment(lib, "Ws2_32.lib")

std::string getHostname()
{
    char hostname[128] = { '\0' };
    gethostname(hostname, 128);
    return hostname;
}
}
