#ifndef __GST_BLOCKINGQUEUE_H__
#define __GST_BLOCKINGQUEUE_H__

#include <condition_variable>
#include <queue>

template <typename T>
class BlockingQueue {

public:
    static BlockingQueue<T>& instance()
    {
        static BlockingQueue<T> obj;
        return obj;
    }

    T take()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_notEmpty.wait(lock, [this] { return !this->m_queue.empty(); });
        T ret(std::move(m_queue.front()));
        m_queue.pop();
        return ret;
    }

    bool tryTake(T& obj, int timeout)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_notEmpty.wait_for(lock, std::chrono::milliseconds(timeout), [this] { return !this->m_queue.empty(); })) {
            return false;
        } else {
            obj = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }
    }

    void put(const T& x)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.size() >= m_capacity){
            m_queue.pop();
        }
        m_queue.push(x);
        m_notEmpty.notify_one();
    }

    size_t size()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    void setCapacity(size_t capacity)
    {
        m_capacity = capacity;
    }

    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;

private:
    BlockingQueue() = default;
    std::mutex m_mutex;
    std::condition_variable m_notEmpty;
    std::queue<T> m_queue;
    size_t m_capacity { 3 };
};

#endif
