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
    
class UniteHelper
{
public:

    UniteHelper(WorkloadID id, std::string graphName, std::string graphPath, int32_t inputSizeNN, int32_t outputSize,
                int32_t _remoteMemoryFd = 0);
    UniteHelper() = default;

    void callInferenceOnBlobs();
    void setup();
    void update(int32_t videoWidth, int32_t videoHeight, int32_t fd = 0ul, 
                const std::vector<ROI>& vecROI = std::vector<ROI>{});

    const std::vector<DetectedObject_t>& getVecObjects() const noexcept
    {
        return _vecObjects;
    }

    const std::vector<std::string>& getVecLabel() const noexcept
    {
        return _vecLabel;
    }

    const std::vector<float>& getVecConfidence() const noexcept
    {
        return _vecConfidence;
    }

    const std::vector<int>& getVecIdx() const noexcept
    {
        return _vecIdx;
    }

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