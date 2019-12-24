//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_THREADUTILS_LINUX_H
#define HDDLUNITE_THREADUTILS_LINUX_H

#include <sys/types.h>

namespace ThreadUtils {
long getThreadId();
pid_t getProcessId();
}

#endif //HDDLUNITE_THREADUTILS_LINUX_H
