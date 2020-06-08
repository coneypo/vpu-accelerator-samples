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
        std::string model;                  //Model path
        std::string iePluginName {"HDDL2"}; //IE plugin name, not used
        unsigned batchSize {1};             //Batch size, not used
        unsigned inferReqNumber {2};        //Infer request number
        float threshold {0.6f};             //Detection confidence threshold
    };

    /**
     * @brief Constructor
     * @param inPortNum Internal parameter handled by hva framework
     * @param outPortNum Internal parameter handled by hva framework
     * @param totalThreadNum Internal parameter handled by hva framework
     * @param vWID Workload id vector
     * @param graphPath Grap file path
     * @param mode Inference mode, currently support "detection"/"classification"
     * @param postproc Postprocess function pointer
     * @param numInferRequest Infer request number
     * @param thresholdDetection Detection threshold
     */
    ImgInferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum,
            std::vector<WorkloadID> vWID, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc, int32_t numInferRequest = 1, float thresholdDetection = 0.6f);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    std::vector<WorkloadID> m_vWID;
    std::string m_graphPath;
    std::string m_mode;
    HDDL2pluginHelper_t::PostprocPtr_t m_postproc;
    
    int32_t m_numInferRequest{1};
    float m_thresholdDetection{0.6f};

    mutable std::atomic<int32_t> m_cntNodeWorker{0};
    mutable std::mutex m_mutex;
};

/**
 * Inference node using IE HDDL2 plugin api
 */
class ImgInferNodeWorker : public hva::hvaNodeWorker_t{
public:
    /**
     * @brief Constructor
     * @param parentNode Internal parameter handled by hva framework
     * @param id Workload id
     * @param graphPath Graph file path
     * @param mode Inference mode, currently support "detection"/"classification"
     * @param postproc Postprocess function pointer
     * @param numInferRequest Infer request number
     * @param thresholdDetection Detection threshold
     */
	ImgInferNodeWorker(hva::hvaNode_t *parentNode, WorkloadID id, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc, int32_t numInferRequest = 1, float thresholdDetection = 0.6f);

	/**
     * @brief Called by hva framework for each video frame, Run inference and pass output to following node
     * @param batchIdx Internal parameter handled by hvaframework
     */
    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

private:
    HDDL2pluginHelper_t m_helperHDDL2;
    // std::vector<ROI> m_vecROI;
    std::string m_mode;

    float m_fps {0.0f};
    float m_durationAve {0.0f};
    uint64_t m_cntFrame {0ul};

//    std::atomic<int32_t> m_cntAsyncEnd{0};
//    std::atomic<int32_t> m_cntAsyncStart{0};

    int32_t m_numInferRequest{1};
    float m_thresholdDetection{0.6f};

private:
    // std::vector<std::shared_ptr<hva::hvaBlob_t>> m_vecBlobInput;
};

#endif //#ifndef INFERNODE_HPP
