//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_LOG_H
#define HDDLUNITE_LOG_H

#include <iostream>
#include <sstream>
#include <string>

#include "utils/CompileUtils.h"
#include "utils/FileUtils.h"
#include "utils/Mutex.h"
#include "utils/Singleton.h"
#include "utils/StringUtils.h"
#include "utils/ThreadUtils.h"
#include "utils/TimeUtils.h"

#ifdef COLOR_LOG
#define COL(x) "\033[1;" #x ";40m"
#define COL_END "\033[0m"
#else
#define COL(x) ""
#define COL_END ""
#endif

#define RED COL(31)
#define GREEN COL(32)
#define YELLOW COL(33)
#define BLUE COL(34)
#define MAGENTA COL(35)
#define CYAN COL(36)
#define WHITE COL(0)
#define DEFAULT_COLOR ""

namespace HddlUnite {
enum class LogLevel : uint32_t {
    FREQUENT = 0x01,
    PROCESS = 0x02,
    DEBUG = 0x04,
    INFO = 0x08,
    WARN = 0x10,
    ERROR = 0x40,
    FATAL = 0x80
};

class Log : public Singleton<Log> {
public:
    void setPrefix(std::string prefix);
    void setSuffix(std::string suffix);
    void setLogLevel(uint32_t logLevel);

    template <typename... Args>
    void doLog(bool on, bool isTraceCallStack, LogLevel level, const char* levelStr, const char* file,
        const char* func, long line, const char* tag, const char* fmt, Args... args);

private:
    Log();
    friend Singleton<Log>;
    static std::string colorBegin(LogLevel logLevel);
    static std::string colorEnd(LogLevel logLevel);

private:
    Mutex mutex;
    std::string prefix;
    std::string suffix;
    uint32_t logLevel;

    static uint32_t defaultLogLevel;
};

inline Log::Log()
    : logLevel(defaultLogLevel)
{
}

inline void Log::setPrefix(std::string prefix_)
{
    AutoMutex autoLock(mutex);
    prefix = std::move(prefix_);
}

inline void Log::setSuffix(std::string suffix_)
{
    AutoMutex autoLock(mutex);
    suffix = std::move(suffix_);
}

inline void Log::setLogLevel(uint32_t logLevel_)
{
    AutoMutex autoLock(mutex);
    logLevel = logLevel_;
}
template <typename... Args>
inline void Log::doLog(bool on, bool isTraceCallStack, LogLevel level, const char* levelStr, const char* file,
    const char* func, const long line, const char* tag, const char* fmt, Args... args)
{
    if (!on || !(static_cast<uint32_t>(level) & static_cast<uint32_t>(logLevel))) {
        return;
    }

    AutoMutex autoLock(mutex);

    std::stringstream stream;
    stream << colorBegin(level) << prefix << '[' << TimeUtils::getCurrentTime() << ']';

#ifdef VERBOSE_LOG
    stream << "[" << ThreadUtils::getThreadId() << "][" << levelStr << "]["
           << ::FileUtils::getFileName(file) << ':' << func << ':' << line << ']';
#else
    stream << '[' << ThreadUtils::getThreadId() << ']';
    if (level < LogLevel::ERROR) {
        stream << levelStr[0];
    } else {
        stream << levelStr;
    }
    stream << '[' << ::FileUtils::getFileName(file) << ':' << line << ']';
#endif

    if (isTraceCallStack) {
        stream << '[' << func << '(' << ')' << ']';
    }
    if (tag) {
        stream << '[' << tag << ']';
    }
    stream << ' ' << StringUtils::format(fmt, args...) << suffix << colorEnd(level);
    std::cout << stream.str() << std::endl;
}

inline std::string Log::colorBegin(HddlUnite::LogLevel logLevel)
{
    if (logLevel == LogLevel::WARN) {
        return std::string(CYAN);
    }
    if (logLevel == LogLevel::ERROR || logLevel == LogLevel::FATAL) {
        return std::string(RED);
    }
    return std::string(DEFAULT_COLOR);
}

inline std::string Log::colorEnd(HddlUnite::LogLevel logLevel)
{
    if (logLevel == LogLevel::WARN || logLevel == LogLevel::ERROR || logLevel == LogLevel::FATAL) {
        return std::string(COL_END);
    }
    return {};
}
}

#endif //HDDLUNITE_LOG_H
