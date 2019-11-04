#ifndef HVA_HVATASK_HPP
#define HVA_HVATASK_HPP

#include <vector>
#include <inc/api/hvaDevice.hpp>
// #include <inc/api/hvaNode.hpp>
#include <inc/scheduler/hvaBarrier.hpp>
#include <inc/util/hvaUtil.hpp>
#include <memory>
#include <unordered_map>
#include <functional>

namespace hva {

enum hvaTaskStatus_t{
    HVA_TASK_SUBMIT = 0,
    HVA_TASK_DISPATCH,
    HVA_TASK_PROCESS,
    HVA_TASK_FINISH
};

enum hvaTimeStampKey_t{
    HVA_KEY_TASK_SUBMIT = 0,
    HVA_KEY_TASK_DISPATCH,
    HVA_KEY_TASK_PROCESS,
    HVA_KEY_TASK_FINISH,
    HVA_KEY_TASK_DURATION
};

}

namespace std {

template <>
class hash<hva::hvaTimeStampKey_t>{
public :
    size_t operator()(const hva::hvaTimeStampKey_t &key ) const
    {
        return hash<int>()(key);
    }
};

};

namespace hva{

//#define HVA_CPU 0x1
//#define HVA_GPU 0x2
//#define HVA_VPU 0x4
//#define HVA_FPGA 0x8
typedef long long hvaMilliSec_t;

class hvaNodeWorker_t;

class hvaSchedulerHint_t{
public:
    hvaDeviceId_t deviceId;
    size_t size;

    std::shared_ptr<hvaTask_t> dependentTask;
};


class hvaTask_t{
public:
    friend hvaScheduler_t;
    using hvaMapKey2Time_t = std::unordered_map< hvaTimeStampKey_t, hvaMilliSec_t>;

    hvaTask_t();
    virtual hvaStatus_t prepare(); // param??
    virtual hvaStatus_t run(hvaDeviceId_t device , void* pParam) = 0;

    hvaTaskStatus_t getTaskStatus() const; //called by user

    virtual hvaStatus_t updateTaskStatus(hvaTaskStatus_t taskStatus); //update by scheduler

    hvaMapKey2Time_t getProfiling() const; //called by user application

    // virtual hvaStatus_t updateProfiling(); // written by user? called by scheduler

    // hvaMeta_t m_meta; //context, other?

    std::vector<hvaSchedulerHint_t>& getSchedulerHints() const;

    std::vector<hvaDeviceId_t> getAvailableDevice() const;
    hvaStatus_t setAvailableDevice(std::vector<hvaDeviceId_t> vAvailableDevice);

    hvaMemoryAffinity_t getConvertCost(hvaDeviceId_t);

protected:

private:
    std::vector<hvaSchedulerHint_t> m_hints;

    std::shared_ptr<hvaNodeWorker_t> m_pNode;

    hvaDeviceId_t m_executeDevice; //execute device

    std::vector<hvaTaskStatus_t> m_taskStatus; //processing status
    // std::vector<std::chrono::time_point> m_vTimestamp;//submit time, dispatch time, start time, finish time
    hvaMapKey2Time_t m_Timestamps;
    std::vector<hvaBarrier_t> m_vbarrier;

    std::vector<hvaDeviceId_t> m_vAvailableDevice;

    std::mutex m_mutex;
    // hvaProfilingInfo_t m_profilingInfo // HW related info used by HW api
    //UserData_t m_data;
};

class hvaTaskClosure_t
{
public:
    hvaTaskClosure_t();
    virtual hvaStatus_t run();
protected:

private:
    std::shared_ptr<hvaTask_t> m_pTask;
};

}



#endif //#ifndef HVA_HVATASK_HPP