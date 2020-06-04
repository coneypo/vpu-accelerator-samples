#ifndef INFERNODE_UNITE_HPP
#define INFERNODE_UNITE_HPP

#include <string>

#include <hvaPipeline.hpp>

#include <common.hpp>
#include "object_selector.hpp"
#include "unite_helper.hpp"

class InferNode_unite : public hva::hvaNode_t{
public:
    /**
     * @brief Constructor
     * @param inPortNum Internal parameter handled by hva framework
     * @param outPortNum Internal parameter handled by hva framework
     * @param totalThreadNum Internal parameter handled by hva framework
     * @param vWID Workload id vector
     * @param graphPath Graph path
     * @param graphName Graph name
     * @param inputSizeNN NN input size
     * @param outputSize NN output size
     */
    InferNode_unite(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, 
    std::vector<WorkloadID> vWID, std::string graphPath, std::string graphName, int32_t inputSizeNN, int32_t outputSize);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    std::vector<WorkloadID> vWID;
    std::string graphName;
    std::string graphPath;
    int32_t inputSizeNN {0};
    int32_t outputSize {0};
    mutable std::atomic_int m_cntNodeWorker {0};
    mutable std::mutex m_mutex;
};

/**
 * Inference node using HDDL unite api
 */
class InferNodeWorker_unite : public hva::hvaNodeWorker_t{
public:
    /**
     * @brief Constructor
     * @param parentNode Internal parameter handled by hva framework
     * @param id Workload id
     * @param graphName Graph name
     * @param graphPath Graph path
     * @param inputSizeNN NN input size
     * @param outputSize NN output size
     */
    InferNodeWorker_unite(hva::hvaNode_t* parentNode,
    WorkloadID id, std::string graphName, std::string graphPath, int32_t inputSizeNN, int32_t outputSize);
    /**
     * @brief Called by hva framework for each video frame, Run inference and pass output to following node
     * @param batchIdx Internal parameter handled by hva framework
     */
    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

private:
    ObjectSelector::Ptr m_object_selector;
    std::vector<std::shared_ptr<hva::hvaBlob_t>> m_vecBlobInput;

    hva::UniteHelper m_uniteHelper;
    std::vector<ROI> m_vecROI;
    std::string m_mode;

    float m_fps {0.0f};
    float m_durationAve {0.0f};
    uint64_t m_cntFrame {0ul};
};

#endif //#ifndef INFERNODE_HPP