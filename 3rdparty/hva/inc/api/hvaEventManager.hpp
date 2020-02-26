#ifndef HVA_HVAEVENTMANAGER_HPP
#define HVA_HVAEVENTMANAGER_HPP

#include <inc/api/hvaEvent.hpp> 
#include <inc/util/hvaUtil.hpp>
#include <mutex>
#include <unordered_map>
#include <condition_variable>
#include <unordered_set>

namespace hva{

class hvaEventManager_t{
public:
    hvaEventManager_t();

    hvaStatus_t registerEvent(hvaEvent_t event);

    hvaStatus_t registerCallback(hvaEvent_t event, hvaEventHandlerFunc callback);

    hvaStatus_t emitEvent(hvaEvent_t event, void* data);

    hvaStatus_t waitForEvent(hvaEvent_t event);
private:
    std::unordered_multimap<hvaEvent_t,hvaEventHandlerFunc> m_callbackMap;
    std::unordered_set<hvaEvent_t> m_eventSet;
    hvaEvent_t m_currentEvent;
    std::mutex m_eventMapMtx;

    std::mutex m_eventBlockingMtx;
    std::condition_variable m_eventBlockingCv;

};

}

#endif //#ifndef HVA_HVAEVENTMANAGER_HPP