//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_FPSCOUNTER_H
#define HDDLUNITE_FPSCOUNTER_H

#include <chrono>
#include <iostream>
#include <list>
#include <sstream>

#include "Mutex.h"
#include "utils/StringUtils.h"

namespace HddlUnite {
class FpsCounter {
public:
    explicit FpsCounter(const std::string& category = "CategoryUnknown", const std::string& object = "ObjectUnknown",
        const std::string& indicator = "Fps");

    void setBufferTimeLength(long bufferTime);
    void reset();

    void add();
    float getFpsValue();

protected:
    void popExpired(std::chrono::steady_clock::time_point& now);

private:
    Mutex m_mutex;
    std::string m_description;
    std::chrono::milliseconds m_timeRange;
    std::list<std::chrono::steady_clock::time_point> m_timePoints;
};

inline FpsCounter::FpsCounter(const std::string& category, const std::string& object, const std::string& indicator)
    : m_timeRange(1000)
{
    std::stringstream ss;

    ss << (category.empty() ? "CategoryUnknown" : category) << "::"
       << (object.empty() ? "ObjectUnknown" : object) << "::"
       << (indicator.empty() ? "Fps" : indicator);

    m_description = ss.str();
}

inline void FpsCounter::setBufferTimeLength(long bufferTime)
{
    m_timeRange = std::chrono::milliseconds(bufferTime);
}

inline void FpsCounter::reset()
{
    AutoMutex autoLock(m_mutex);

    m_timePoints.clear();
}

inline void FpsCounter::add()
{
    AutoMutex autoLock(m_mutex);

    auto now = std::chrono::steady_clock::now();

    if (!m_timePoints.empty()) {
        popExpired(now);
    }

    m_timePoints.push_back(now);
}

inline float FpsCounter::getFpsValue()
{
    AutoMutex autoLock(m_mutex);

    auto now = std::chrono::steady_clock::now();

    popExpired(now);

    if (m_timePoints.size() < 2) {
        return 0.0f;
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_timePoints.front());
    if (duration > m_timeRange) {
        duration = m_timeRange;
    }

    float fps = (m_timePoints.size() * 1000.0f) / duration.count();

    return fps;
}

inline void FpsCounter::popExpired(std::chrono::steady_clock::time_point& now)
{
    while (!m_timePoints.empty()) {
        auto front = m_timePoints.front();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - front);
        if (duration < m_timeRange) {
            break;
        }

        m_timePoints.pop_front();
    }
}
}

#endif //HDDLUNITE_FPSCOUNTER_H
