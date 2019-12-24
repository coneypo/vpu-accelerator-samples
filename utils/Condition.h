//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_CONDITION_H
#define HDDLUNITE_CONDITION_H

#include <chrono>
#include <condition_variable>

#include "utils/Mutex.h"

namespace HddlUnite {
class Condition {
public:
    void wait(Mutex& mutex);
    bool waitFor(Mutex& mutex, long milliseconds);
    void signal();
    void broadcast();

private:
    std::condition_variable m_condition;
};

inline void Condition::wait(Mutex& mutex)
{
    std::unique_lock<std::mutex> lock(mutex.m_mutex, std::defer_lock);
    m_condition.wait(lock);
}

inline bool Condition::waitFor(Mutex& mutex, long milliseconds)
{
    std::unique_lock<std::mutex> lock(mutex.m_mutex, std::defer_lock);

    if (milliseconds < 0) {
        m_condition.wait(lock);
        return true;
    }

    auto status = m_condition.wait_for(lock, std::chrono::milliseconds(milliseconds));

    if (status == std::cv_status::timeout) {
        errno = ETIME;
        return false;
    }

    return true;
}

inline void Condition::signal()
{
    m_condition.notify_one();
}

inline void Condition::broadcast()
{
    m_condition.notify_all();
}
}

#endif //HDDLUNITE_CONDITION_H
