#ifndef HVA_HVANODE_HPP
#define HVA_HVANODE_HPP

#include <memory>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <inc/api/hvaTask.hpp>
#include <inc/api/hvaBlob.hpp>
#include <inc/scheduler/hvaScheduler.hpp>
#include <inc/util/hvaUtil.hpp>
// #include <inc/api/hvaPipeline.hpp>

#include <TestConfig.h>

namespace hva{

enum hvaPortPolicy_t{
    HVA_BLOCKING_IF_FULL = 0,
    HVA_DISCARD_IF_FULL
};

using hvaConvertFunc = std::function<std::shared_ptr<hvaBlob_t>(std::shared_ptr<hvaBlob_t>)>;

class hvaInPort_t;
class hvaOutPort_t;
class hvaNode_t;

// using ms = std::chrono::milliseconds; // moved to hvautil.hpp
using hvaDataQueue_t = std::list<std::shared_ptr<hvaBlob_t>>;

class hvaInPort_t{
friend class hvaPipeline_t;
friend class hvaNode_t;
friend class hvaBatchingConfig_t;
public:
    hvaInPort_t(hvaNode_t* parentNode, hvaOutPort_t* prevPort, size_t queueSize = 8);

    hvaInPort_t(hvaNode_t* parentNode);

    std::shared_ptr<hvaBlob_t>& front();

    void pop();

    hvaStatus_t tryPush(std::shared_ptr<hvaBlob_t> data);

    hvaStatus_t push(std::shared_ptr<hvaBlob_t> data, ms timeout = ms(0));

    void setPortQueuePolicy(hvaPortPolicy_t policy);

#ifdef KL_TEST
public:
#else
private:
#endif
    void setPrevPort(hvaOutPort_t* prevPort);

    std::mutex m_mutex;
    std::condition_variable m_cv_empty;
    std::condition_variable m_cv_full;
    const size_t m_queueSize; // todo: impl a setQueueSize and make queuesize non-const
    hvaPortPolicy_t m_policy;
    hvaNode_t* m_parentNode;
    hvaOutPort_t* m_prevPort;
    hvaDataQueue_t m_inQueue;
};

class hvaPipeline_t;

class hvaOutPort_t{
// friend void hvaPipeline_t::linkNode(hvaNode_t* prev, hvaNode_t* next);
friend class hvaPipeline_t;
public:
    hvaOutPort_t(hvaNode_t* parentNode, hvaInPort_t* nextPort);

    hvaOutPort_t(hvaNode_t* parentNode);

    hvaInPort_t* getNextPort();

    std::shared_ptr<hvaBlob_t> convert(std::shared_ptr<hvaBlob_t>) const;

    bool isConvertValid() const;
#ifdef KL_TEST
public:
#else
private:
#endif
    void setNextPort(hvaInPort_t* nextPort);

    void setConvertFunc(hvaConvertFunc func);

    hvaNode_t* m_parentNode;
    hvaInPort_t* m_nextPort;
    hvaConvertFunc m_convertFunc;
};

class hvaTaskConfig_t{
public:
    hvaTaskConfig_t();

    virtual ~hvaTaskConfig_t();
};

class hvaStreamInfo_t{
public:
    hvaStreamInfo_t();

    virtual ~hvaStreamInfo_t();

    size_t streamId;
};

class hvaBatchingConfig_t{
public:
    enum BatchingPolicy : unsigned{
        BatchingIgnoringStream = 0x1,
        BatchingWithStream = 0x2,
        Reserved = 0x4
    };

    hvaBatchingConfig_t();

    unsigned batchingPolicy;
    std::size_t batchSize;
    std::size_t streamNum;
    std::size_t threadNumPerBatch;

/**
* @brief    function pointer to batching algorithm
*           batching algorithm could be default batching or provided by user
*           batching algorithm should control mutex and cv
*
* @param    (batch index, vector of port index, pointer to node)
* @return   output blob vector(when batching fail, vector is empty)
*
* @auther
*
**/
    std::function<std::vector<std::shared_ptr<hvaBlob_t> > (std::size_t batchIdx, std::vector<std::size_t> vPortIdx, hvaNode_t* pNode)> batchingAlgo = nullptr;

    static std::vector<std::shared_ptr<hvaBlob_t> > defaultBatching(std::size_t batchIdx, std::vector<std::size_t> vPortIdx, hvaNode_t* pNode);

    static std::vector<std::shared_ptr<hvaBlob_t> > streamBatching(std::size_t batchIdx, std::vector<std::size_t> vPortIdx, hvaNode_t* pNode);
};

class hvaNodeWorker_t;

class hvaNode_t{
friend class hvaPipeline_t;
friend class hvaBatchingConfig_t;
friend class hvaInPort_t;
public:
    
    hvaNode_t(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum);

    // obsoleted hvaNode_t(std::size_t totalThreadNum);

    // obsoleted hvaNode_t(std::size_t streamNum, std::size_t batchNum, std::size_t threadNumPerBatch);

    virtual ~hvaNode_t();

    /**
    * @brief this function constructs and returns a nodeworker instance. Users are expected to 
    *   implement this function while extending node data structure. Aditionally users may pass 
    *   the members m_vpTaskConfig and one of the m_vpStreamInfo to the nodeworker constucted using 
    *   the static int m_workerCtr. e.g. m_vpStreamInfo[m_workerCtr++]
    * @param 
    * @param 
    * @return 
    *
    * @auther KL 
    * 
    */
    virtual std::shared_ptr<hvaNodeWorker_t> createNodeWorker() const = 0;

    // obsoleted virtual void config(std::vector<std::shared_ptr<hvaTaskConfig_t>> taskConfig, std::vector<std::shared_ptr<hvaStreamInfo_t>> streamInfo);
    virtual void configBatch(const hvaBatchingConfig_t& config);
    virtual void configBatch(hvaBatchingConfig_t&& config);

    void configLoopingInterval(ms interval);

    virtual std::vector<std::shared_ptr<hvaBlob_t>> getBatchedInput(std::size_t batchIdx, std::vector<std::size_t> vPortIdx);

    std::size_t getInPortNum();

    std::size_t getOutPortNum();

    hvaStatus_t sendOutput(std::shared_ptr<hvaBlob_t> data, std::size_t portId, ms timeout);

    std::size_t getTotalThreadNum();

    const hvaBatchingConfig_t* getBatchingConfigPtr();

    const hvaBatchingConfig_t& getBatchingConfig();

    ms getLoopingInterval() const;
    
    void stopBatching();


#ifdef KL_TEST
public:
#else
private:
#endif
    static std::size_t m_workerCtr;

    hvaOutPort_t& getOutputPort(std::size_t portIdx);
    hvaInPort_t& getInputPort(std::size_t portIdx);
    
    std::vector<std::unique_ptr<hvaInPort_t>> m_inPorts;
    std::vector<std::unique_ptr<hvaOutPort_t>> m_outPorts;

    // obsoleted std::vector<std::shared_ptr<hvaTaskConfig_t>> m_vpTaskConfig;
    // obsoleted std::vector<std::shared_ptr<hvaStreamInfo_t>> m_vpStreamInfo;
    const std::size_t m_inPortNum;
    const std::size_t m_outPortNum;
    hvaBatchingConfig_t m_batchingConfig;
    std::unordered_map<int, int> m_mapStream2LastFrameId;
    std::size_t m_totalThreadNum;

    ms m_loopingInterval;
    std::mutex m_batchingMutex;
    std::condition_variable m_batchingCv;
    std::atomic_bool m_batchingStoped;

};

class hvaNodeWorker_t{ //ar: renamed NodeWorker_t
public:
    hvaNodeWorker_t(hvaNode_t* parentNode);

    virtual ~hvaNodeWorker_t();

    virtual void process(std::size_t batchIdx) = 0;
    virtual void init();
    virtual void deinit();
    // void submitTask();

    hvaNode_t* getParentPtr() const;

    bool isStopped() const;
protected:
    // void submitTask(hvaTask_t& task);
    hvaStatus_t sendOutput(std::shared_ptr<hvaBlob_t> data, std::size_t portId, ms timeout = ms(1000));

    void breakProcessLoop();
#ifdef KL_TEST
public:
#else
private:
#endif
    hvaNode_t* m_parentNode;
    volatile bool m_internalStop;
    // hvaScheduler_t& m_sche;
};


}


#endif //#ifndef HVA_HVANODE_HPP