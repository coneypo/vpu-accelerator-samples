#pragma once
#include <vector>
#include <memory>
#include <string>
#include <iostream>

#include <ie_compound_blob.h>
#include <ie_iextension.h>
#include <opencv2/opencv.hpp>
#include "RemoteMemory.h"
#include "InferGraph.h"
#include <InferBlob.h>
#include "WorkloadContext.h"
#include <HddlUnite.h>
#include <Inference.h>

#include "common.hpp"
#include "detection_helper.hpp"
#include "tinyYolov2_post.h"
#include "ImageNetLabels.hpp"

#define MAX_ROI_UNITE 10ul
namespace hva
{
/**
 * Wrapper for HDDL unite inference API
 */
class UniteHelper
{
public:
    /**
     * @brief Constructor
     * @param id Workload ID
     * @param graphName Graph name
     * @param graphPath File path of model graph
     * @param inputSizeNN NN input size
     * @param outputSize NN output size
     * @param remoteMemoryFd Remote memory FD
     */
    UniteHelper(WorkloadID id, std::string graphName, std::string graphPath, int32_t inputSizeNN, int32_t outputSize,
                int32_t remoteMemoryFd = 0);
    UniteHelper() = delete;

    /**
     * @brief run sync inference
     */
    void callInferenceOnBlobs();
    void setup();
    /**
     * @brief update parameters for inference
     * @param videoWidth Video width
     * @param videoHeight Video height
     * @param fd Remote memory FD
     * @param vecROI ROI vector
     */
    void update(int32_t videoWidth, int32_t videoHeight, int32_t fd = 0ul, 
                const std::vector<ROI>& vecROI = std::vector<ROI>{});

    /**
     * @brief Get detected objects
     * @return Detected objects vector
     */
    const std::vector<DetectedObject_t>& getVecObjects() const noexcept
    {
        return _vecObjects;
    }

    /**
     * @brief Get classification labels
     * @return Classification label vector
     */
    const std::vector<std::string>& getVecLabel() const noexcept
    {
        return _vecLabel;
    }

    /**
     * @brief Get classification confidences
     * @return Classification confidence vector
     */
    const std::vector<float>& getVecConfidence() const noexcept
    {
        return _vecConfidence;
    }

    /**
     * @brief Get classification label IDs
     * @return Classification label ID vector
     */
    const std::vector<int>& getVecIdx() const noexcept
    {
        return _vecIdx;
    }

    /**
     * @brief Get graph name
     * @return Graph name
     */
    const std::string& getGraphName() const noexcept
    {
        return _graphName;
    }

protected:
    WorkloadID _workloadId = -1;
    int32_t _remoteMemoryFd = -1;

    HddlUnite::SMM::RemoteMemory::Ptr _remoteMemoryPtr = nullptr;
    HddlUnite::Inference::InferData::Ptr _inferDataPtr = nullptr;
    HddlUnite::Inference::Graph::Ptr _graphPtr = nullptr;
    std::vector<HddlUnite::Inference::AuxBlob::Type> _auxBlob {HddlUnite::Inference::AuxBlob::Type::TimeTaken};

    ImageNetLabels _labels;

    std::vector<DetectedObject_t> _vecObjects;
    std::vector<int> _vecIdx;
    std::vector<std::string> _vecLabel;
    std::vector<float> _vecConfidence;

    std::string _graphName;
    std::string _graphPath;
    std::string _inputName = "input_name";
    std::string _outputName = "output_name";
    int32_t _inputSizePP;
    int32_t _inputSizeNN;
    int32_t _outputSize;

    std::string _inputPath;
    HddlUnite::Inference::Precision _precision = HddlUnite::Inference::Precision::U8;
    int32_t _videoWidth;
    int32_t _videoHeight;
    int32_t _videoWidthStride;

    bool _needPP = true;
};

} //namespace hva