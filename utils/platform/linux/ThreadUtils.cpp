//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "ThreadUtils.h"
#include "utils/ThreadUtils.h"

namespace ThreadUtils {
long getThreadId()
{
    return syscall(SYS_gettid);
}

pid_t getProcessId()
{
    return getpid();
}

void setName(const char* pname)
{
    prctl(PR_SET_NAME, pname);
}

uint64_t getRdtsc()
{
    uint64_t tstamp { 0 };

#if defined(__x86_64__)
    asm("rdtsc\n\t"
        "shlq   $32,%%rdx\n\t"
        "or     %%rax,%%rdx\n\t"
        "movq   %%rdx,%0\n\t"
        : "=g"(tstamp)
        :
        : "rax", "rdx");
#elif defined(__i386__)
    asm("rdtsc\n"
        : "=A"(tstamp));
#else
#endif

    return tstamp;
}

void getName(char* name, size_t size)
{
    if (name && size >= 16) {
        prctl(PR_GET_NAME, name);
    }
}

std::string getHostname()
{
    char hostname[128] = { '\0' };
    gethostname(hostname, sizeof(hostname));
    return hostname;
}
}