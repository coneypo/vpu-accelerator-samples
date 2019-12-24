//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_THREADPOOL_H
#define HDDLUNITE_THREADPOOL_H

#include <functional>
#include <future>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "Condition.h"
#include "Mutex.h"
#include "ThreadUtils.h"

namespace HddlUnite {
class ThreadPool {
public:
    explicit ThreadPool(std::string poolName = "ThreadPool", size_t nThreads = 3, size_t capacity = 20);
    ~ThreadPool();

    template <class Func, class... Args>
    auto enqueue(Func&& func, Args&&... args)
        -> std::future<decltype(func(args...))>;

    template <class Func, class... Args>
    auto enqueueFront(Func&& func, Args&&... args)
        -> std::future<decltype(func(args...))>;

    void flush();

protected:
    void createThreads();
    void exitAllThreads();
    void setThreadName(size_t id);
    void enqueueTask(std::function<void()>&& task, bool isUrgent);
    bool dequeueTask(std::function<void()>& task);

private:
    Mutex m_mutex;
    std::string m_poolName;
    bool m_stopRunning;
    Condition m_threadSleep;
    Condition m_queueNotEmpty;
    Condition m_queueNotFull;
    Condition m_allTasksDone;

    std::list<std::function<void()>> m_waitTasks;
    size_t m_capacity;

    std::list<std::thread> m_threads;
    size_t m_numThreads;
    size_t m_numWaitThreads;
    size_t m_numStoppedThreads;
};

inline ThreadPool::ThreadPool(std::string poolName, size_t nThreads, size_t capacity)
    : m_poolName(std::move(poolName))
    , m_stopRunning(false)
    , m_capacity(capacity)
    , m_numThreads(nThreads)
    , m_numWaitThreads(0)
    , m_numStoppedThreads(0)
{
    createThreads();
}

inline ThreadPool::~ThreadPool()
{
    exitAllThreads();

    for (auto& worker : m_threads) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

template <class Func, class... Args>
auto ThreadPool::enqueue(Func&& func, Args&&... args)
    -> std::future<decltype(func(args...))>
{
    using retType = decltype(func(args...));

    auto task = std::make_shared<std::packaged_task<retType()>>(
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

    enqueueTask([=]() { (*task)(); }, false);

    return task->get_future();
}

template <class Func, class... Args>
auto ThreadPool::enqueueFront(Func&& func, Args&&... args)
    -> std::future<decltype(func(args...))>
{
    using retType = decltype(func(args...));

    auto task = std::make_shared<std::packaged_task<retType()>>(
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

    enqueueTask([=]() { (*task)(); }, true);

    return task->get_future();
}

inline void ThreadPool::flush()
{
    AutoMutex autoLock(m_mutex);

    while (m_numWaitThreads + m_numStoppedThreads != m_numThreads) {
        m_threadSleep.waitFor(m_mutex, 1000);
    }
}

inline void ThreadPool::createThreads()
{
    for (size_t i = 0; i < m_numThreads; i++) {
        m_threads.emplace_back(
            [=]() {
                setThreadName(i);

                std::function<void()> task;
                while (dequeueTask(task)) {
                    task();
                }
            });
    }
}

inline void ThreadPool::exitAllThreads()
{
    AutoMutex autoLock(m_mutex);

    m_stopRunning = true;

    m_queueNotEmpty.broadcast();
}

inline void ThreadPool::setThreadName(size_t id)
{
    auto threadName = m_poolName + 'T' + std::to_string(id);

    ThreadUtils::setName(threadName);
}

inline void ThreadPool::enqueueTask(std::function<void()>&& task, bool isUrgent)
{
    AutoMutex autoLock(m_mutex);

    while (!m_stopRunning && m_waitTasks.size() >= m_capacity) {
        m_queueNotFull.wait(m_mutex);
    }

    if (m_stopRunning) {
        throw std::runtime_error("failed to enqueue, threadPool is stopped");
    }

    if (isUrgent) {
        m_waitTasks.emplace_front(task);
    } else {
        m_waitTasks.emplace_back(task);
    }

    m_queueNotEmpty.signal();
}

inline bool ThreadPool::dequeueTask(std::function<void()>& task)
{
    AutoMutex autoLock(m_mutex);

    while (!m_stopRunning && m_waitTasks.empty()) {
        ++m_numWaitThreads;
        m_threadSleep.signal();
        m_queueNotEmpty.wait(m_mutex);
        --m_numWaitThreads;
    }

    if (m_stopRunning && m_waitTasks.empty()) {
        ++m_numStoppedThreads;
        m_threadSleep.signal();
        return false;
    }

    task = std::move(m_waitTasks.front());

    m_waitTasks.pop_front();
    m_queueNotFull.signal();

    return true;
}
}

#endif //HDDLUNITE_THREADPOOL_H
