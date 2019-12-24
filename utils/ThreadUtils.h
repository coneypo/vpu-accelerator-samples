//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_THREADUTILS_H
#define HDDLUNITE_THREADUTILS_H

#include <cstdint>
#include <string>

#ifdef WIN32
#include "platform/windows/ThreadUtils.h"
#else
#include "platform/linux/ThreadUtils.h"
#endif

namespace ThreadUtils {
uint64_t getRdtsc();
std::string getName();
std::string getHostname();

void setName(const char* name);
void setName(const std::string& name);
void getName(char* name, size_t size);
void saveThreadInfo(const std::string& threadName, const std::string& saveFile = "threads.info");
}

#endif //HDDLUNITE_THREADUTILS_H
