#ifndef OBJECT_TRACKING_NODE_HPP
#define OBJECT_TRACKING_NODE_HPP

#include <string>
#include <hvaPipeline.hpp>
#include "common.hpp"
#include "HddlUnite.h"
#include "vas/ot.h"

class ObjectTrackingNode : public hva::hvaNode_t{
public:
    /**
     * @brief Constructor
     * @param inPortNum Internal parameter handled by hva framework
     * @param outPortNum Internal parameter handled by hva framework
     * @param totalThreadNum Internal parameter handled by hva framework
     * @param vWorkloadId Workload id vector
     * @param reservedInt Reserved integer
     * @param reservedStr Reserved string
     * @param trackingModeStr Tracking mode
     */
    ObjectTrackingNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, 
        std::vector<WorkloadID> vWorkloadId, int32_t reservedInt,
        std::string reservedStr, const std::string& trackingModeStr);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    mutable int32_t m_cntNodeWorker{0};

    std::vector<WorkloadID> m_vWorkloadId;
    int32_t m_reservedInt{0};
    std::string m_reservedStr;
    std::string m_trackingModeStr;  // ZERO_TERM_IMAGELESS or SHORT_TERM_IMAGELESS
};

class ObjectTrackingNodeWorker : public hva::hvaNodeWorker_t{
public:
    /**
     * @brief Constructor
     * @param parentNode Internal parameter handled by hva framework
     * @param workloadId Workload id
     * @param reservedInt Reserved integer
     * @param reservedStr Reserved string
     * @param trackingModeStr Tracking mode
     */
    ObjectTrackingNodeWorker(hva::hvaNode_t* parentNode,
        WorkloadID workloadId, int32_t reservedInt,
        std::string reservedStr, const std::string& trackingModeStr);

    /**
     * @brief Called by hva framework for each video frame, Run object tracking algorithm and pass output to following node
     * @param batchIdx Internal parameter handled by hvaframework
     */
    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

    WorkloadID m_workloadId{0ul};
    float m_fps{0.0f};
    float m_durationAve {0.0f};
    uint64_t m_cntFrame{0ul};

    cv::Mat m_dummy;
    std::shared_ptr<vas::ot::ObjectTracker> m_tracker;

    int32_t m_reservedInt{0};
    std::string m_reservedStr;
    std::string m_trackingModeStr;

private:

    std::vector<std::shared_ptr<hva::hvaBlob_t>> m_vecBlobInput;
};

#endif //#ifndef OBJECT_TRACKING_NODE_HPP