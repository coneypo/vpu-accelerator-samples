#ifndef HVA_HVADEVICE_HPP
#define HVA_HVADEVICE_HPP

#include <vector>
#include <list>
#include <string>
#include <inc/util/hvaUtil.hpp>
#include <chrono>
#include <memory>
#include <inc/scheduler/hvaThreadPool.hpp>
#include <inc/util/hvaThreadSafeQueue.hpp>

namespace hva{


class hvaScheduler_t;
class hvaTask_t;
class hvaTaskClosure_t;


typedef std::string hvaDeviceType_t; // string or uuid?
typedef unsigned short hvaDeviceIndex_t;
typedef unsigned int hvaMemoryAffinity_t; // bigger affinity means more time to transfer data
// using hvaTaskClosure_t = std::pair<std::shared_ptr<hvaTask_t>, void*>;
// using hvaTaskQueue_t = std::list<hvaTaskClosure_t>;


class hvaDeviceId_t{
public:
    hvaDeviceId_t();
    hvaDeviceType_t m_type;
    hvaDeviceIndex_t m_index;
protected:

private:
};


// class hvaTaskQueue_t{
// public:
//     hvaStatus_t pop(std::shared_ptr<hvaTaskClosure_t>&);
//     hvaStatus_t push(std::shared_ptr<hvaTaskClosure_t>);
//     hvaStatus_t close();

// private:
//     hvaThreadSafeQueue_t<std::shared_ptr<hvaTaskClosure_t> > m_queue;
//     std::shared_ptr<hvaDevice_t> m_pDevice;
// };


class hvaDevice_t: std::enable_shared_from_this<hvaDevice_t>{
public:
    friend hvaScheduler_t;
    static hvaDevice_t& getInstance();

    virtual hvaStatus_t configDevice(std::size_t threadPoolSize, std::size_t maxQueueSize) = 0;

    virtual std::chrono::milliseconds profiling(); //parameter?

    virtual hvaDeviceType_t getDeviceType() = 0;

    hvaStatus_t setDeviceId(hvaDeviceId_t id);
    hvaStatus_t getDeviceId(hvaDeviceId_t& id) const;

    hvaStatus_t getHostAffinity(hvaMemoryAffinity_t& aff) const;
    hvaStatus_t setHostAffinity(hvaMemoryAffinity_t aff);

    hvaStatus_t popTask(std::shared_ptr<hvaTaskClosure_t>& pTaskClosure);
    hvaStatus_t pushTask(const std::shared_ptr<hvaTaskClosure_t>& pTaskClosure);
    hvaStatus_t close();

protected:

private:
    hvaDevice_t();
    ~hvaDevice_t();
    hvaDevice_t(const hvaDevice_t &) = delete;
    hvaDevice_t &operator =(const hvaDevice_t &) = delete;

    std::size_t m_threadPoolSize;
    std::size_t m_maxQueueSize;

    hvaMemoryAffinity_t m_hostAffinity;

    // std::shared_ptr<hvaTaskQueue_t> m_TaskQueues;
    hvaThreadSafeQueue_t<std::shared_ptr<hvaTaskClosure_t> > m_TaskQueues;
    std::shared_ptr<hvaThreadPool_t> m_ThreadPool;

    //UserData userdata;
    //std::size_t m_resourceLimit;
};

}


namespace std {

template <>
class hash<hva::hvaDeviceId_t>{
public :
    size_t operator()(const hva::hvaDeviceId_t &key ) const
    {
        return (hash<hva::hvaDeviceType_t>()(key.m_type) 
                ^ hash<hva::hvaDeviceIndex_t>()(key.m_index));
    }
};

};

#endif //#ifndef HVA_HVADEVICE_HPP