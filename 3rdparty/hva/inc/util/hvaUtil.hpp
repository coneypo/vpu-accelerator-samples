#ifndef HVA_HVAUTIL_HPP
#define HVA_HVAUTIL_HPP

#include <chrono>

namespace hva{

void hvaAssertFail(const char* cond, int line, const char* file,
                      const char* func);

// #define HVA_ASSERT(cond) if(!cond){ std::cout<<"Assert Failed!"<<std::endl;}
#define HVA_ASSERT(cond) do{if(!(cond)){::hva::hvaAssertFail(#cond, __LINE__, __FILE__, \
    __func__);}} while(false);

enum hvaStatus_t{
    hvaSuccess = 0,
    hvaFailure,
    hvaPortFullDiscarded,
    hvaPortFullTimeout,
    hvaPortNullPtr,
    hvaEventRegisterFailed,
    hvaEventNotFound,
    hvaCallbackFail
};

using ms = std::chrono::milliseconds;

}

#endif //HVA_HVAUTIL_HPP