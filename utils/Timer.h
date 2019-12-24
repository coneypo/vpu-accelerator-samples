//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <mutex>

#include "Semaphore.h"

namespace HddlUnite {
class Timer {
public:
    using Callback = std::function<void()>;

    Timer(Callback callback, size_t intervalTime, bool periodic = false);
    Timer() = default;

    void setCallback(Callback callback) noexcept;
    void setInterval(size_t intervalTimeInMs) noexcept;
    void setRepeated(bool repeated) noexcept;

    void start() noexcept;
    void start(size_t intervalTime) noexcept;
    void stop() noexcept;
    bool isActive() const noexcept;

private:
    using TimePoint = std::chrono::system_clock::time_point;
    using ScheduledTimer = std::pair<TimePoint, Timer>;

    size_t m_id { 0 };
    Callback m_callback;
    bool m_repeated { false };
    size_t m_intervalTime { 0 };

    static std::mutex s_scheduledTimersMutex;
    static std::multimap<std::chrono::system_clock::time_point, Timer> s_scheduledTimers;
    static Semaphore s_newTimerSemaphore;
    static std::atomic<size_t> s_timerIdCounter;

    static ScheduledTimer getFirstTimer() noexcept;
    static ScheduledTimer popFirstTimer() noexcept;
    static void startTicking() noexcept;
};
}
