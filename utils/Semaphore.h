//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_SEMAPHORE_H
#define HDDLUNITE_SEMAPHORE_H

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace HddlUnite {
class Semaphore {
public:
    explicit Semaphore(int initValue = 0);

    void wait();
    void post();

    bool tryWait();
    bool waitFor(long milliseconds);

    int getValue();
    void resetValue(int value);

private:
    std::mutex m_mutex;
    int m_value;
    std::condition_variable m_ready;
};

inline Semaphore::Semaphore(int initValue)
    : m_value(initValue)
{
}

inline void Semaphore::wait()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_ready.wait(lock, [=]() { return m_value > 0; });
    --m_value;
}

inline void Semaphore::post()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    ++m_value;
    m_ready.notify_one();
}

inline bool Semaphore::tryWait()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_value > 0) {
        --m_value;
        return true;
    }
    return false;
}

inline bool Semaphore::waitFor(long milliseconds)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_ready.wait_for(lock, std::chrono::milliseconds(milliseconds), [=]() { return m_value > 0; })) {
        --m_value;
        return true;
    }
    return false;
}

inline int Semaphore::getValue()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_value;
}

inline void Semaphore::resetValue(int value)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_value = value;
}
}

#endif //HDDLUNITE_SEMAPHORE_H
