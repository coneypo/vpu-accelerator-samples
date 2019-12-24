//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <iostream>
#include <thread>

#include "Timer.h"

namespace HddlUnite {
static std::once_flag routineFlag;
std::multimap<Timer::TimePoint, Timer> Timer::s_scheduledTimers;
std::mutex Timer::s_scheduledTimersMutex;
std::atomic<size_t> Timer::s_timerIdCounter { 0 };
Semaphore Timer::s_newTimerSemaphore { 0 };

Timer::Timer(Callback callback, size_t intervalTime, bool periodic)
    : m_callback(callback)
    , m_repeated(periodic)
    , m_intervalTime(intervalTime)
{
}

void Timer::setInterval(size_t intervalTimeInMs) noexcept
{
    m_intervalTime = intervalTimeInMs;
}

void Timer::setCallback(Callback callback) noexcept
{
    m_callback = callback;
}

void Timer::setRepeated(bool repeated) noexcept
{
    m_repeated = repeated;
}

void Timer::start() noexcept
{
    std::call_once(routineFlag, startTicking);
    if (m_id && m_repeated) {
        std::cerr << "This repeated timer has started, please stop first";
        return;
    }

    m_id = ++s_timerIdCounter;
    {
        std::unique_lock<std::mutex> l(s_scheduledTimersMutex);
        s_scheduledTimers.emplace(std::chrono::system_clock::now() + std::chrono::milliseconds(m_intervalTime), *this);
    }
    s_newTimerSemaphore.post();
}

void Timer::start(size_t intervalTime) noexcept
{
    setInterval(intervalTime);
    start();
}

void Timer::stop() noexcept
{
    if (!m_id) {
        std::cerr << "This timer hasn't started, please start first." << std::endl;
        return;
    }
    {
        std::unique_lock<std::mutex> l(s_scheduledTimersMutex);
        for (auto it = s_scheduledTimers.begin(); it != s_scheduledTimers.end(); it++) {
            if (it->second.m_id == m_id) {
                s_scheduledTimers.erase(it);
                m_id = 0;
                s_newTimerSemaphore.post();
                return;
            }
        }
    }
    m_id = 0;
    if (!m_repeated) {
        std::cerr << "this single timer has finished, nothing to stop" << std::endl;
    } else {
        std::cerr << "error can not find the repeated timer" << std::endl;
    }
}

bool Timer::isActive() const noexcept
{
    return m_id;
}

Timer::ScheduledTimer Timer::getFirstTimer() noexcept
{
    ScheduledTimer result;
    std::unique_lock<std::mutex> l(s_scheduledTimersMutex);
    if (!s_scheduledTimers.empty()) {
        result = *(s_scheduledTimers.begin());
    }
    return result;
}

Timer::ScheduledTimer Timer::popFirstTimer() noexcept
{
    ScheduledTimer result;
    std::unique_lock<std::mutex> l(s_scheduledTimersMutex);
    if (!s_scheduledTimers.empty()) {
        result = *(s_scheduledTimers.begin());
        s_scheduledTimers.erase(s_scheduledTimers.begin());
        if (result.second.m_repeated) {
            s_scheduledTimers.emplace(result.first + std::chrono::milliseconds(result.second.m_intervalTime), result.second);
        }
    }
    return result;
}

void Timer::startTicking() noexcept
{
    std::thread routine([] {
        while (true) {
            ScheduledTimer topScheduledTimer = getFirstTimer();
            if (topScheduledTimer.second.m_intervalTime == 0) {
                s_newTimerSemaphore.wait();
                continue;
            }

            auto waitingTime = std::chrono::duration_cast<std::chrono::milliseconds>(topScheduledTimer.first - std::chrono::system_clock::now()).count();
            if (s_newTimerSemaphore.waitFor(static_cast<long>(waitingTime))) {
                continue;
            }

            topScheduledTimer = popFirstTimer();
            if (topScheduledTimer.second.m_intervalTime == 0) {
                continue;
            }

            topScheduledTimer.second.m_callback();
        }
    });
    routine.detach();
}
}
