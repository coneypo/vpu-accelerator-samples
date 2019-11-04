#ifndef HVA_HVABARRIER_H
#define HVA_HVABARRIER_H
#include <mutex>
#include <condition_variable>
#include <memory>

namespace hva
{
class hvaBarrier_t{
public:

    hvaBarrier_t();


protected:

private:

    std::shared_ptr<std::condition_variable> m_cvReady;
    std::shared_ptr<std::mutex> m_mutex;
    bool m_isReady;


};

}


#endif //HVA_HVABARRIER_H
