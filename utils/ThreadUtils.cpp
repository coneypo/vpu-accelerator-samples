//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <fstream>
#include <iomanip>

#include "utils/Mutex.h"
#include "utils/ThreadUtils.h"

namespace ThreadUtils {
void setName(const std::string& name)
{
    setName(name.c_str());
}

std::string getName()
{
    char threadName[32] = { '\0' };
    getName(threadName, 32);
    return threadName;
}

void saveThreadInfo(const std::string& threadName, const std::string& saveFile)
{
    static HddlUnite::Mutex mutex;
    static uint32_t threadCount = 0;

    HddlUnite::AutoMutex autoLock(mutex);

    std::ofstream stream;
    if (!threadCount) {
        stream.open(saveFile, std::ios::out | std::ios::trunc);
    } else {
        stream.open(saveFile, std::ios::out | std::ios::app);
    }

    stream << "[" << std::setw(2) << std::setfill('0') << threadCount++ << "]"
           << " threadId=" << getThreadId() << " threadName=" << threadName << std::endl;
}
}
