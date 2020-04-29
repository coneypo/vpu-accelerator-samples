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
        std::unique_lock<std::mutex> lock(_mutex);
        _nonEmpty.wait(lock, [this] { return !this->_queue.empty(); });
        T ret(std::move(_queue.front()));
        _queue.pop();
        return ret;
    }

    bool tryTake(T& obj, int timeout)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (!_nonEmpty.wait_for(lock, std::chrono::milliseconds(timeout), [this] { return !this->_queue.empty(); })) {
            return false;
        } else {
            obj = std::move(_queue.front());
            _queue.pop();
            return true;
        }
    }

    void put(const T& x)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if ( _queue.size() >= m_capacity){
            _queue.pop();
        }
        _queue.push(x);
        _nonEmpty.notify_one();
    }

    size_t size()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.size();
    }

    void setCapacity(size_t capacity)
    {
        m_capacity = capacity;
    }

    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;

private:
    BlockingQueue() = default;
    std::mutex _mutex;
    std::condition_variable _nonEmpty;
    std::queue<T> _queue;
    size_t m_capacity { 3 };
};

#endif
