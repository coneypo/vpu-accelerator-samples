//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_MUTEX_H
#define HDDLUNITE_MUTEX_H

#include <mutex>

namespace HddlUnite {
class Mutex {
public:
    void lock() { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }
    bool tryLock() { return m_mutex.try_lock(); }

private:
    std::mutex m_mutex;

    friend class Condition;
};

class AutoMutex {
public:
    explicit AutoMutex(Mutex& mutex)
        : m_mutex(mutex)
    {
        m_mutex.lock();
    };

    ~AutoMutex()
    {
        m_mutex.unlock();
    };

private:
    Mutex& m_mutex;
};
}

#endif //HDDLUNITE_MUTEX_H
