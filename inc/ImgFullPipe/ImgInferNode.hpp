#ifndef IMGINFERNODE_HPP
#define IMGINFERNODE_HPP

#include <hvaPipeline.hpp>
#include <string>
#include <common.hpp>
#include "unite_helper.hpp" 
#include "hddl2plugin_helper.hpp"

class ImgInferNode : public hva::hvaNode_t{
public:
    struct Config{
        std::string model; //required
        std::string iePluginName;
        unsigned batchSize;
        unsigned inferReqNumber;
        float threshold;
    };

    ImgInferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum,
            std::vector<WorkloadID> vWID, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    std::vector<WorkloadID> m_vWID;
    std::string m_graphPath;
    std::string m_mode;
    HDDL2pluginHelper_t::PostprocPtr_t m_postproc;
    
    mutable std::atomic<int32_t> m_cntNodeWorker{0};
    mutable std::mutex m_mutex;
};

class ImgInferNodeWorker : public hva::hvaNodeWorker_t{
public:
	ImgInferNodeWorker(hva::hvaNode_t *parentNode, WorkloadID id, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc);

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

private:
    // std::vector<std::shared_ptr<hva::hvaBlob_t>> m_vecBlobInput;
};

#endif //#ifndef INFERNODE_HPP
