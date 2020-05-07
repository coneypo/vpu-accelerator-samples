#ifndef INFERNODE_HPP
#define INFERNODE_HPP

#include <hvaPipeline.hpp>
#include <string>
#include <common.hpp>
#include "unite_helper.hpp" 
#include "hddl2plugin_helper.hpp"
#include "object_selector.hpp"

class InferNode : public hva::hvaNode_t{
public:
    struct Config{
        std::string model; //required
        std::string iePluginName {"HDDL2"};
        unsigned batchSize {1};
        unsigned inferReqNumber {2};
        float threshold {0.6f};
    };

    InferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum,
            std::vector<WorkloadID> vWID, std::string graphPath, std::string mode, 
            HDDL2pluginHelper_t::PostprocPtr_t postproc, int32_t numInferRequest = 1, float thresholdDetection = 0.6f);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    std::vector<WorkloadID> m_vWID;
    std::string m_graphPath;
    std::string m_mode;
    HDDL2pluginHelper_t::PostprocPtr_t m_postproc;
    
    //todo, config
    int32_t m_numInferRequest{1};
    float m_thresholdDetection{0.6f};
    
    mutable std::atomic<int32_t> m_cntNodeWorker{0};
    mutable std::mutex m_mutex;
};

class InferNodeWorker : public hva::hvaNodeWorker_t{
public:
    InferNodeWorker(hva::hvaNode_t *parentNode, WorkloadID id, std::string graphPath, std::string mode, 
                    HDDL2pluginHelper_t::PostprocPtr_t postproc, int32_t numInferRequest = 1, float thresholdDetection = 0.6f);

    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

private:
    HDDL2pluginHelper_t m_helperHDDL2;
    // std::vector<ROI> m_vecROI;
    std::string m_mode;

    float m_fps {0.0f};
    float m_durationAve {0.0f};
    uint64_t m_cntFrame {0ul};

    std::atomic<int32_t> m_cntAsyncEnd{0};
    std::atomic<int32_t> m_cntAsyncStart{0};

    ObjectSelector::Ptr m_object_selector;
    HDDL2pluginHelper_t::OrderKeeper_t m_orderKeeper;
    
    //todo, config
    int32_t m_numInferRequest{1};
    float m_thresholdDetection{0.6f};

private:
    // std::vector<std::shared_ptr<hva::hvaBlob_t>> m_vecBlobInput;
};

#endif //#ifndef INFERNODE_HPP
