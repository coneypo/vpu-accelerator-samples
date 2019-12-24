//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_THREADUTILS_WIN32_H
#define HDDLUNITE_THREADUTILS_WIN32_H

#include <windows.h>

namespace ThreadUtils {
DWORD getThreadId();
DWORD getProcessId();
}

#endif //HDDLUNITE_THREADUTILS_WIN32_H
