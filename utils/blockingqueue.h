#ifndef __GST_BLOCKINGQUEUE_H__
#define __GST_BLOCKINGQUEUE_H__

#include <queue>
#include <condition_variable>


template <typename T>
class BlockingQueue {

public:
  BlockingQueue(): _mutex(), _nonEmpty(), _queue() {}

  T take() {
    std::unique_lock<std::mutex> lock(_mutex);
    _nonEmpty.wait(lock, [this]{ return !this->_queue.empty(); });
    T ret(std::move(_queue.front()));
    _queue.pop();

    return ret;
  }

  void put(const T &x) {
    std::lock_guard<std::mutex> lock(_mutex);
    _queue.push(x);
    _nonEmpty.notify_one();
  }

  size_t size() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _queue.size();
  }

  BlockingQueue(const BlockingQueue&) = delete;
  BlockingQueue& operator=(const BlockingQueue&) = delete;

private:
  std::mutex _mutex;
  std::condition_variable _nonEmpty;
  std::queue<T> _queue;
};

#endif
