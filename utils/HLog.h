//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_HLOG_H
#define HDDLUNITE_HLOG_H

#include <cstdarg>

#include "utils/Log.h"

#define HLogger HddlUnite::Log::instance()

#define HLogPrint(isOn, isTraceCallStack, logLevel, level, tag, ...) \
    HLogger->doLog(isOn, isTraceCallStack, logLevel, level, __FILE__, __func__, __LINE__, tag, __VA_ARGS__)

#define HFrequent(isOn, tag, ...) HLogPrint(isOn, HddlUnite::LogLevel::FREQUENT, "FREQ", tag, __VA_ARGS__)
#define HProcess(isOn, tag, ...) HLogPrint(isOn, false, HddlUnite::LogLevel::PROCESS, "PROC", tag, __VA_ARGS__)
#define HDebug(...) HLogPrint(true, false, HddlUnite::LogLevel::DEBUG, "DEBUG", nullptr, __VA_ARGS__)
#define HInfo(...) HLogPrint(true, false, HddlUnite::LogLevel::INFO, "INFO", nullptr, __VA_ARGS__)
#define HWarn(...) HLogPrint(true, false, HddlUnite::LogLevel::WARN, "WARN", nullptr, __VA_ARGS__)
#define HError(...) HLogPrint(true, false, HddlUnite::LogLevel::ERROR, "ERROR", nullptr, __VA_ARGS__)
#define HFatal(...) HLogPrint(true, false, HddlUnite::LogLevel::FATAL, "FATAL", nullptr, __VA_ARGS__)

#define TraceCallStacks(...) HLogPrint(true, true, HddlUnite::LogLevel::DEBUG, "DEBUG", nullptr, __VA_ARGS__)
#define TraceCallStack() TraceCallStacks(" ")

namespace HddlUnite {
inline void setLogLevel(uint32_t logLevel)
{
    HLogger->setLogLevel(logLevel);
}
}

#endif //HDDLUNITE_HLOG_H
