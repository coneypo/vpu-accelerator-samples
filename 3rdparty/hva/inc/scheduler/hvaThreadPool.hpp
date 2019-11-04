#ifndef HVA_HVATHREADPOOL_HPP
#define HVA_HVATHREADPOOL_HPP

#include <vector>
#include <thread>
#include <atomic>
#include <inc/util/hvaUtil.hpp>

namespace hva
{
class hvaTask_t;
class hvaDevice_t;

class hvaThreadPool_t
{
public:
    hvaThreadPool_t(size_t threadNum);
    hvaThreadPool_t() = delete;
    ~hvaThreadPool_t() = default;
    hvaThreadPool_t(const hvaThreadPool_t&) = delete;
    hvaThreadPool_t& operator= (const hvaThreadPool_t&) = delete;

    void threadWorker();
    hvaStatus_t start();
    hvaStatus_t stop();

protected:

private:
    std::shared_ptr<hvaDevice_t> m_pDevice;
    std::vector<std::thread> m_vThread;
    size_t m_threadNum;
    std::atomic<bool> m_done;
};


    
} // namespace hva



#endif //#ifndef HVA_HVATHREADPOOL_HPP
