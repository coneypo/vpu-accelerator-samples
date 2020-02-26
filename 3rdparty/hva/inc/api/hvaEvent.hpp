#ifndef HVA_HVAEVENT_HPP
#define HVA_HVAEVENT_HPP

#include <cstdint>
#include <functional>

namespace hva{

typedef uint64_t hvaEvent_t;

#define hvaEvent_Null 0x0ull

using hvaEventHandlerFunc = std::function<bool(void*)>;

}

#endif //#ifndef HVA_HVAEVENT_HPP
