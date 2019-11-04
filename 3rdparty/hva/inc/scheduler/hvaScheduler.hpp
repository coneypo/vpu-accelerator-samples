#ifndef HVA_HVASCHEDULER_HPP
#define HVA_HVASCHEDULER_HPP

#include <vector>
#include <list>
#include <memory>
#include <unordered_map>
#include <inc/api/hvaDevice.hpp>
#include <inc/api/hvaTask.hpp>
#include <inc/util/hvaUtil.hpp>
#include <inc/scheduler/hvaBarrier.hpp>

namespace hva{

typedef unsigned int hvaSchedulePolicy_t;
class hvaBarrier_t;
class hvaTask_t;
class hvaDevice_t;

class hvaScheduler_t{
public:

    static hvaScheduler_t& getInstance();

    hvaBarrier_t submitTask(std::shared_ptr<hvaTask_t>, std::vector<hvaBarrier_t> vBarrier = std::vector<hvaBarrier_t>(), void* pParam = NULL); //

    hvaStatus_t addDevice(hvaDeviceId_t deviceId, std::shared_ptr<hvaDevice_t> deviceInfo); //change string to uuid

    hvaStatus_t waitForMultiBarrier(std::vector<hvaBarrier_t> vBarrier, bool waitforall, std::chrono::milliseconds timeout);

    hvaStatus_t waitForSingleBarrier(hvaBarrier_t barrier, std::chrono::milliseconds timeout);

    hvaStatus_t setPolicy(hvaSchedulePolicy_t schedulePolicy);
    hvaStatus_t getPolicy(hvaSchedulePolicy_t& schedulePolicy) const;

protected:

private:
    hvaScheduler_t() = default;
    ~hvaScheduler_t() = default;
    hvaScheduler_t(const hvaScheduler_t &) = delete;
    hvaScheduler_t &operator =(const hvaScheduler_t &) = delete;

    hvaStatus_t dispatchTask(hvaTaskClosure_t); //**

    std::list<hvaTaskClosure_t> m_lpWaitingTasks;

    using hvaMapId2Device_t = std::unordered_map< hvaDeviceId_t, std::shared_ptr<hvaDevice_t> >;
    hvaMapId2Device_t m_mapId2Device;
    hvaSchedulePolicy_t m_schedulePolicy;
};



enum hvaSchedulePolicyEnum_t{
    HVA_POLICY_AVAILABLE_DEVICE = 0x0001,
    HVA_POLICY_IDLE_DEVICE = 0x0002,
    HVA_POLICY_PROFILING_INFO = 0x0004,
    HVA_POLICY_CONVERT_COST = 0x0008,
    DEFAULT_POLICY = 0x000F
};

}

#endif //#ifndef HVA_HVASCHEDULER_HPP