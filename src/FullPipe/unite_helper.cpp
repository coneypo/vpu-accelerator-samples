#include "unite_helper.hpp"
#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include <fstream>

#include <opencv2/opencv.hpp>
#include "ie_layouts.h"
#include "RemoteMemory.h"
#include "InferGraph.h"
#include <InferBlob.h>
#include "WorkloadContext.h"
#include <HddlUnite.h>
#include <Inference.h>
#include "hvaLogger.hpp"

#include "detection_helper.hpp"
#include "tinyYolov2_post.h"
#include "hddl2plugin_helper.hpp"


template <typename TIterator>
auto product(TIterator beg, TIterator en) -> typename std::remove_reference<decltype(*beg)>::type {
    return std::accumulate(beg, en, static_cast<typename std::remove_reference<decltype(*beg)>::type>(1),
                           std::multiplies<typename std::remove_reference<decltype(*beg)>::type>());
}

namespace hva
{
std::vector<std::string> readLabelsFromFile(const std::string& labelFileName) {
    std::vector<std::string> labels;

    std::ifstream inputFile;
    inputFile.open(labelFileName, std::ios::in);
    if (inputFile.is_open()) {
        std::string strLine;
        while (std::getline(inputFile, strLine)) {
            labels.push_back(strLine);
        }
    }
    return labels;
}

template<int N>
int alignTo(int x)
{
    return ((x + N - 1) & ~(N - 1));
}

void UniteHelper::setup() {

    _videoHeight = alignTo<64>(_videoHeight);
    _videoWidth = alignTo<64>(_videoWidth);
    _videoWidthStride = _videoWidth;
    HddlUnite::WorkloadContext::Ptr context = HddlUnite::queryWorkloadContext(_workloadId);
    _inferDataPtr = HddlUnite::Inference::makeInferData(_auxBlob, context, MAX_ROI_UNITE);

    HddlStatusCode statusCode = HddlUnite::Inference::loadGraph(
            _graphPtr, _graphName, _graphPath, {*context});
    if (statusCode != HDDL_OK) {
        throw std::runtime_error("Failed to load graph");
    }

    if (_remoteMemoryFd <= 0)
    {
        _remoteMemoryPtr = HddlUnite::SMM::allocate(*context, _inputSizePP);
        _remoteMemoryFd = _remoteMemoryPtr->getDmaBufFd();
    }

    auto inputBlob = _inferDataPtr->getInputBlob(_inputName);
    if (inputBlob == nullptr) {
        try {
            const int isInput = true;

            const bool isRemoteMem = true;
            const bool needAllocate = false;

            HddlUnite::Inference::BlobDesc blobDesc(_precision, isRemoteMem, needAllocate);
            HddlUnite::Inference::Rectangle rect {0, 0, _videoWidth, _videoHeight};
            blobDesc.m_fd = _remoteMemoryFd;
            blobDesc.m_resWidth = _videoWidth;
            blobDesc.m_resHeight = _videoHeight;
            blobDesc.m_widthStride = _videoWidthStride;
            blobDesc.m_planeStride = _videoWidthStride * _videoHeight;
            blobDesc.m_rect.push_back(rect);
            if(!_inferDataPtr->createBlob(_inputName, blobDesc, isInput))
            {
                HVA_DEBUG("error!\n");
            }

            _inferDataPtr->setPPFlag(_needPP);
            if (_needPP)
            {
                // need pp
                // set nn input desc to inferData, only need for video workload
                // we will allocate its RemoteMemory, all parameters can be got from network parser
                HddlUnite::Inference::NNInputDesc nnInputDesc {
                    _precision, isRemoteMem, true, static_cast<uint64_t>(_inputSizeNN)};
                _inferDataPtr->setNNInputDesc(nnInputDesc);

                // set PP shave.
                _inferDataPtr->setPPShaveNumber(4);
            }   
        } catch (const std::exception& ex) {
            throw std::runtime_error("Failed to create default input blob");
        }
    }

    auto outputBlob = _inferDataPtr->getOutputBlob(_outputName);
    if (outputBlob == nullptr) {
        try {
            const int isInput = false;
            const bool isRemoteMem = true;
            const bool needAllocate = true;

            HddlUnite::Inference::BlobDesc blobDesc(_precision, isRemoteMem, needAllocate, _outputSize);
            if (!_inferDataPtr->createBlob(_outputName, blobDesc, isInput))
            {
                HVA_DEBUG("error!\n");
            }

        } catch (const std::exception& ex) {
            throw std::runtime_error("Failed to create default output blob");
        }
    }
}
void UniteHelper::callInferenceOnBlobs() {

    FILE* fp;

    auto graph = _graphPtr;
    HddlStatusCode inferStatus = HddlUnite::Inference::inferSync(*graph, _inferDataPtr);

    auto& outputsInfo = _inferDataPtr->getOutBlobs();
    for (auto& item : outputsInfo) {
        assert(item.second != nullptr);
        auto memoryBuffer = item.second->getData();
        assert(memoryBuffer.size() > 0);

        InferenceEngine::TensorDesc desc(InferenceEngine::Precision::U8,
            {1, 1, 1, memoryBuffer.size()}, InferenceEngine::Layout::NHWC);
        
        InferenceEngine::Blob::Ptr ptrBlob = InferenceEngine::make_shared_blob<uint8_t>(desc, const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(memoryBuffer.data())));
        HVA_DEBUG("output size is %ld\n", memoryBuffer.size());

        //post processing
        if ("resnet" == _graphName || "classification" == _graphName)
        {
            auto blobDequant = HDDL2pluginHelper_t::deQuantize(ptrBlob, 0.151837, 67);
            auto* ptrFP32 = blobDequant->buffer().as<float*>();

            std::vector<int>& vecIdx = _vecIdx;
            std::vector<std::string>& vecLabel = _vecLabel;
            std::vector<float>& vecConfidence = _vecConfidence;
            vecIdx.clear();
            vecLabel.clear();
            vecConfidence.clear();
            uint64_t sizeYoloOutput = 21125;
            for (int i = 0; i < memoryBuffer.size() / sizeYoloOutput; i++)
            {
                float* ptrFP32_ROI = ptrFP32 + _outputSize * i;
                float max = 0.0f;
                float sum = 0.0f;
                int idx = 0;
                for (int j = 0; j < _outputSize; j++)
                {
                    sum += exp(ptrFP32_ROI[j]);
                    if (ptrFP32_ROI[j] > max)
                    {
                        idx = j;
                        max = ptrFP32_ROI[j];
                    }
                }
                vecIdx.push_back(idx);
                vecLabel.push_back(_labels.imagenet_labelstring(idx));
                vecConfidence.push_back(exp(max) / sum);
                HVA_DEBUG("roi label is : %s\n", _labels.imagenet_labelstring(idx).c_str());
            }
        }
        if ("yolotiny" == _graphName || "detection" == _graphName)
        {
            auto blobDequant = HDDL2pluginHelper_t::deQuantize(ptrBlob, 0.271045, 210);

            IE::Blob::Ptr detectResult = HDDL2pluginHelper_t::yoloLayer_yolov2tiny(blobDequant, _videoHeight, _videoWidth);

            _vecObjects.clear();

            size_t N = detectResult->getTensorDesc().getDims()[2];
            if (detectResult->getTensorDesc().getDims()[3] != 7)
            {
                throw std::logic_error("Output item should have 7 as a last dimension");
            }
            const float *rawData = detectResult->cbuffer().as<const float *>();

            // imageid,labelid,confidence,x0,y0,x1,y1
            for (size_t i = 0; i < N; i++)
            {
                if (rawData[i * 7 + 2] > 0.001)
                {
                    DetectedObject_t object;

                    int x1,y1,x2,y2,w,h;
                    x1 = (rawData[i * 7 + 3]) * _videoWidth / 416;
                    y1 = (rawData[i * 7 + 4]) * _videoHeight / 416;
                    w = (rawData[i * 7 + 5] - rawData[i * 7 + 3]) * _videoWidth / 416;
                    h = (rawData[i * 7 + 6] - rawData[i * 7 + 4]) * _videoHeight / 416;
                    object.confidence = rawData[i * 7 + 2];
                    
                    x2 = (x1 + w) & (~1);
                    y2 = (y1 + h) & (~1);
                    x1 = x1 & (~1);
                    y1 = y1 & (~1);

                    object.x = x1;
                    object.y = y1;
                    object.width = x2 - x1;
                    object.height = y2 - y1;

                    _vecObjects.push_back(object);
                }
            }

            for (auto& object : _vecObjects)
            {
                HVA_DEBUG("object detected: x is %d, y is %d, w is %d, h is %d\n", object.x, object.y, object.width, object.height);
            }
        }
    }
}

UniteHelper::UniteHelper(WorkloadID id, std::string graphName, std::string graphPath, int32_t inputSizeNN, int32_t outputSize,
                        int32_t remoteMemoryFd):
                    _workloadId(id), _graphName(graphName), _graphPath(graphPath), _remoteMemoryFd(remoteMemoryFd),
                     _inputSizeNN(inputSizeNN), _outputSize(outputSize)
{
}

void UniteHelper::update(int32_t videoWidth, int32_t videoHeight, int32_t fd, const std::vector<ROI>& vecROI)
{
    videoWidth = alignTo<64>(videoWidth);
    videoHeight = alignTo<64>(videoHeight);
    this->_videoWidth = videoWidth;
    this->_videoHeight = videoHeight;
    if (0 != fd)
    {
        _remoteMemoryFd = fd;
    }

    _inferDataPtr->setPPFlag(_needPP);

    if (_needPP)
    {
        _inputSizePP = videoWidth * videoHeight * 3 / 2;
    }
    else
    {
        _inputSizePP = videoWidth * videoHeight * 3;
    }
    _videoWidthStride = videoWidth; 

    auto inputBlob = _inferDataPtr->getInputBlob(_inputName);

    const int isInput = true;

    const bool isRemoteMem = true;
    const bool needAllocate = false;

    HddlUnite::Inference::BlobDesc blobDesc(_precision, isRemoteMem, needAllocate);

    blobDesc.m_fd = _remoteMemoryFd;
    blobDesc.m_resWidth = videoWidth;
    blobDesc.m_resHeight = videoHeight;
    blobDesc.m_widthStride = _videoWidthStride;
    blobDesc.m_planeStride = videoWidth * videoHeight;

    if (vecROI.size() > 0)
    {
        for (size_t i = 0; i < std::min(vecROI.size(), MAX_ROI_UNITE); i++)
        {
            auto& roi = vecROI[i];
            HddlUnite::Inference::Rectangle rect {roi.x, roi.y, roi.width, roi.height};
            blobDesc.m_rect.push_back(rect);
        }
    }
    else
    {
        HddlUnite::Inference::Rectangle rect {0, 0, videoWidth, videoHeight};
        blobDesc.m_rect.push_back(rect);
    }
    
    inputBlob->updateBlob(blobDesc);   
}
} //namespace hva
