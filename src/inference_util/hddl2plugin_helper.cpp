#include "hddl2plugin_helper.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <fstream>
#include <memory>
#include <chrono>
#include <map>
#include <set>
#include <condition_variable>

#include <inference_engine.hpp>
#include <ie_compound_blob.h>
#include <hddl2_params.hpp>
#include <opencv2/opencv.hpp>
#include <WorkloadContext.h>
#include <HddlUnite.h>
#include <RemoteMemory.h>

#include "hvaLogger.hpp"
#include "common.hpp"
#include "tinyYolov2_post.h"
#include "region_yolov2tiny.h"
#include "detection_helper.hpp"
#include "ImageNetLabels.hpp"
#include "hvaLogger.hpp"

namespace IE = InferenceEngine;

ImageNetLabels HDDL2pluginHelper_t::m_labels;

void HDDL2pluginHelper_t::setupImgPipe(int32_t numInferRequest) {
    const std::string &modelPath = _graphPath;

    if (_graphPath.find(".blob") != std::string::npos) {
        std::filebuf blobFile;
        if (!blobFile.open(modelPath, std::ios::in | std::ios::binary)) {
            blobFile.close();
            THROW_IE_EXCEPTION<< "Could not open file: " << modelPath;
        }
        std::istream graphBlob(&blobFile);
        _executableNetwork = _ie.ImportNetwork(graphBlob, "HDDL2");
    }
    else
    {
        auto network = _ie.ReadNetwork(modelPath, modelPath.substr(0, modelPath.length() - 4) + ".bin");

        const auto in_precision = InferenceEngine::Precision::U8;
        for (auto &&layer : network.getInputsInfo()) {
            layer.second->setLayout(InferenceEngine::Layout::NHWC);
            layer.second->setPrecision(in_precision);
        }

        const auto out_precision = InferenceEngine::Precision::FP16;
        for (auto &&layer : network.getOutputsInfo()) {
            if (layer.second->getDims().size() == 2) {
                layer.second->setLayout(InferenceEngine::Layout::NC);
            } else {
                layer.second->setLayout(InferenceEngine::Layout::NHWC);
            }
            layer.second->setPrecision(out_precision);
        }

//            _executableNetwork = _ie.LoadNetwork(network, "HDDL2");
        _executableNetwork = _ie.LoadNetwork(network, "HDDL2", {});
    }

    // ---- Create infer request
    // _ptrInferRequest = _executableNetwork.CreateInferRequestPtr();
    _ptrInferRequestPool = std::make_shared<InferRequestPool_t>(
            _executableNetwork, numInferRequest);
}
void HDDL2pluginHelper_t::inferAsyncImgPipe(IE::InferRequest::Ptr ptrInferRequest, InferCallback_t callback,
            char* hostFd, size_t heightInput, size_t widthInput, const ROI &roi)
{
    if (_heightInput != heightInput)
    {
        _heightInput = heightInput;
    }
    if (_widthInput != widthInput)
    {
        _widthInput = widthInput;
    }

    IE::ROI roiIE;
    if(0 == roi.width || 0 == roi.height)
    {
        roiIE = IE::ROI{0, 0, 0, widthInput, heightInput};
    }
    else
    {
        roiIE = IE::ROI{static_cast<size_t>(0), static_cast<size_t>(roi.x),
                        static_cast<size_t>(roi.y), static_cast<size_t>(roi.width), static_cast<size_t>(roi.height)};
    }

    if (_executableNetwork.GetInputsInfo().empty())
    {
        throw std::runtime_error("input info empty");
    }

    auto inputsInfo = _executableNetwork.GetInputsInfo();
    _inputName = _executableNetwork.GetInputsInfo().begin()->first;


    // ---- Resize preprocessing also required
    IE::PreProcessInfo preprocInfo = ptrInferRequest->GetPreProcess(
            _inputName);
    preprocInfo.setResizeAlgorithm(IE::RESIZE_BILINEAR);
    preprocInfo.setColorFormat(IE::ColorFormat::NV12);

    // Create a blob to hold the NV12 input data
    // Create tensor descriptors for Y and UV blobs
    IE::TensorDesc y_plane_desc(IE::Precision::U8, { 1, 1, heightInput,
            widthInput }, IE::Layout::NHWC);
    IE::TensorDesc uv_plane_desc(IE::Precision::U8,
            { 1, 2, heightInput / 2, widthInput / 2 }, IE::Layout::NHWC);
    const size_t offset = widthInput * heightInput;

    // Create blob for Y plane from raw data
    IE::Blob::Ptr y_blob = IE::make_shared_blob<uint8_t>(y_plane_desc,
            reinterpret_cast<uint8_t*>(hostFd));
    // Create blob for UV plane from raw data
    IE::Blob::Ptr uv_blob = IE::make_shared_blob<uint8_t>(uv_plane_desc,
            reinterpret_cast<uint8_t*>(hostFd) + offset);
    // Create NV12Blob from Y and UV blobs
    IE::Blob::Ptr nv12InputBlob = IE::make_shared_blob<IE::NV12Blob>(y_blob,
            uv_blob);

    ptrInferRequest->SetBlob(_inputName, nv12InputBlob, preprocInfo);

    if (_executableNetwork.GetOutputsInfo().empty())
    {
        throw std::runtime_error("output info empty");
    }
    auto outputBlobName = _executableNetwork.GetOutputsInfo().begin()->first;
    auto outputBlob = ptrInferRequest->GetBlob(outputBlobName);
    auto& desc = outputBlob->getTensorDesc();

    desc.setPrecision(IE::Precision::FP32);

    ptrInferRequest->SetCompletionCallback(callback);

    //Async model
    ptrInferRequest->StartAsync();

    //sync model,callback() be used in inferNode
//        ptrInferRequest->Infer();

    return;
}

void HDDL2pluginHelper_t::setup(int32_t numInferRequest)
{
    // ---- Load inference engine instance
    HVA_DEBUG("start load graph\n");

    // ---- Init context map and create context based on it
    IE::ParamMap paramMap = {{IE::HDDL2_PARAM_KEY(WORKLOAD_CONTEXT_ID), _workloadId}};
    _ptrContextIE = _ie.CreateContext("HDDL2", paramMap);

    // ---- Import network providing context as input to bind to context
    const std::string &modelPath = _graphPath;

    if (_graphPath.find(".blob") != std::string::npos)
    {
        std::filebuf blobFile;
        if (!blobFile.open(modelPath, std::ios::in | std::ios::binary))
        {
            blobFile.close();
            throw std::runtime_error("Could not open file");
        }
        std::istream graphBlob(&blobFile);
        _executableNetwork = _ie.ImportNetwork(graphBlob, _ptrContextIE);
    }
    else
    {
        auto network = _ie.ReadNetwork(modelPath, modelPath.substr(0, modelPath.length() - 4) + ".bin");

        const auto in_precision = InferenceEngine::Precision::U8;
        for (auto &&layer : network.getInputsInfo())
        {
            layer.second->setLayout(InferenceEngine::Layout::NHWC);
            layer.second->setPrecision(in_precision);
        }

        const auto out_precision = InferenceEngine::Precision::FP16;
        for (auto &&layer : network.getOutputsInfo())
        {
            if (layer.second->getDims().size() == 2)
            {
                layer.second->setLayout(InferenceEngine::Layout::NC);
            }
            else
            {
                layer.second->setLayout(InferenceEngine::Layout::NHWC);
            }
            layer.second->setPrecision(out_precision);
        }

        _executableNetwork = _ie.LoadNetwork(network, _ptrContextIE, {});
    }
    HVA_DEBUG("end load graph\n");

    // ---- Create infer request
    _ptrInferRequestPool = std::make_shared<InferRequestPool_t>(_executableNetwork, numInferRequest);
}

void HDDL2pluginHelper_t::inferAsync(IE::InferRequest::Ptr ptrInferRequest, InferCallback_t callback,
                        int remoteMemoryFd, size_t heightInput, size_t widthInput, const ROI &roi)
{
    if (_heightInput != heightInput)
    {
        _heightInput = heightInput;
    }
    if (_widthInput != widthInput)
    {
        _widthInput = widthInput;
    }

    // ---- Create remote blob by using already exists fd
    assert(remoteMemoryFd >= 0);

    IE::ROI roiIE;
    if (0 == roi.width || 0 == roi.height)
    {
        roiIE = IE::ROI{0, 0, 0, widthInput, heightInput};
    }
    else
    {
        roiIE = IE::ROI{static_cast<size_t>(0), static_cast<size_t>(roi.x),
                        static_cast<size_t>(roi.y), static_cast<size_t>(roi.width), static_cast<size_t>(roi.height)};
    }
    IE::ParamMap blobParamMap = {{IE::HDDL2_PARAM_KEY(REMOTE_MEMORY_FD), static_cast<uint64_t>(remoteMemoryFd)},
                                    {IE::HDDL2_PARAM_KEY(COLOR_FORMAT), IE::ColorFormat::NV12},
                                    {IE::HDDL2_PARAM_KEY(ROI), roiIE}};

    if (_executableNetwork.GetInputsInfo().empty())
    {
        throw std::runtime_error("input info empty");
    }

    auto inputsInfo = _executableNetwork.GetInputsInfo();
    _inputName = _executableNetwork.GetInputsInfo().begin()->first;

    IE::TensorDesc inputTensorDesc = IE::TensorDesc(IE::Precision::U8, {1, 3, heightInput, widthInput}, IE::Layout::NCHW);
    auto ptrRemoteBlob = _ptrContextIE->CreateBlob(inputTensorDesc, blobParamMap);

    assert(nullptr != ptrRemoteBlob);

    //Resize preprocessing also required
    IE::PreProcessInfo preprocInfo = ptrInferRequest->GetPreProcess(_inputName);
    preprocInfo.setResizeAlgorithm(IE::RESIZE_BILINEAR);
    preprocInfo.setColorFormat(IE::ColorFormat::NV12);

    // ---- Set remote NV12 blob with preprocessing information
    ptrInferRequest->SetBlob(_inputName, ptrRemoteBlob, preprocInfo);

    if (_executableNetwork.GetOutputsInfo().empty())
    {
        throw std::runtime_error("output info empty");
    }
    auto outputBlobName = _executableNetwork.GetOutputsInfo().begin()->first;
    auto outputBlob = ptrInferRequest->GetBlob(outputBlobName);
    auto &desc = outputBlob->getTensorDesc();

    desc.setPrecision(IE::Precision::FP32);

    HVA_DEBUG("end create input tensor\n");

    // ---- Run the request asynchronously
    HVA_DEBUG("start inference\n");

    ptrInferRequest->Wait(1000000);
    ptrInferRequest->SetCompletionCallback(callback);
    ptrInferRequest->StartAsync();

    HVA_DEBUG("end inference\n");

    return;
}

IE::Blob::Ptr HDDL2pluginHelper_t::getOutputBlob(IE::InferRequest::Ptr ptrInferRequest)
{
    // --- Get output
    HVA_DEBUG("start dump output file\n");

    if (_executableNetwork.GetOutputsInfo().empty())
    {
        throw std::runtime_error("output info empty");
    }

    auto outputBlobName = _executableNetwork.GetOutputsInfo().begin()->first;
    auto ptrOutputBlob = ptrInferRequest->GetBlob(outputBlobName);

    HVA_DEBUG("end dump output file\n");

    return ptrOutputBlob;
}

IE::InferRequest::Ptr HDDL2pluginHelper_t::getInferRequest()
{
    return _ptrInferRequestPool->get();
}
void HDDL2pluginHelper_t::putInferRequest(IE::InferRequest::Ptr ptrInferRequest)
{
    _ptrInferRequestPool->put(ptrInferRequest);
    return;
}

void HDDL2pluginHelper_t::postproc(IE::Blob::Ptr ptrBlob, std::vector<ROI> &vecROI)
{
    if (nullptr != _ptrPostproc)
    {
        _ptrPostproc(this, ptrBlob, vecROI);
    }
    return;
}

void HDDL2pluginHelper_t::postprocYolotinyv2_fp16(IE::Blob::Ptr ptrBlob, std::vector<ROI> &vecROI)
{
    HVA_DEBUG("detection confidence threshold is %f", _thresholdDetection);
    std::vector<DetectedObject_t> vecObjects;
    vecObjects = ::YoloV2Tiny::TensorToBBoxYoloV2TinyCommon(ptrBlob, _heightInput, _widthInput,
                                                            _thresholdDetection, YoloV2Tiny::fillRawNetOut);

    vecROI.clear();
    for (auto &object : vecObjects)
    {
        ROI roi;
        roi.x = object.x;
        roi.y = object.y;
        roi.height = object.height;
        roi.width = object.width;
        roi.confidenceDetection = object.confidence;
        roi.labelIdDetection = object.labelId;
        vecROI.push_back(roi);
    }

    for (auto &object : vecObjects)
    {
        HVA_DEBUG("object detected: x is %d, y is %d, w is %d, h is %d\n", object.x, object.y, object.width, object.height);
    }

    return;
}

void HDDL2pluginHelper_t::postprocYolotinyv2_u8(IE::Blob::Ptr ptrBlob, std::vector<ROI> &vecROI)
{
    auto blobDequant = deQuantize(ptrBlob, 0.271045, 210);

    IE::Blob::Ptr detectResult = yoloLayer_yolov2tiny(blobDequant, _heightInput, _widthInput);

    vecROI.clear();

    size_t N = detectResult->getTensorDesc().getDims()[2];
    if (detectResult->getTensorDesc().getDims()[3] != 7)
    {
        throw std::logic_error("Output item should have 7 as a last dimension");
    }
    const float *rawData = detectResult->cbuffer().as<const float *>();

    // imageid,labelid,confidence,x0,y0,x1,y1
    for (size_t i = 0; i < N; i++)
    {
        if (rawData[i * 7 + 2] > 0.1f)
        {
            ROI object;
            object.x = (rawData[i * 7 + 3]) * _widthInput / 416;
            object.y = (rawData[i * 7 + 4]) * _heightInput / 416;
            object.width = (rawData[i * 7 + 5] - rawData[i * 7 + 3]) * _widthInput / 416;
            object.height = (rawData[i * 7 + 6] - rawData[i * 7 + 4]) * _heightInput / 416;
            object.confidenceDetection = rawData[i * 7 + 2];
            vecROI.push_back(object);
        }
    }
    return;
}

void HDDL2pluginHelper_t::postprocResnet50_fp16(IE::Blob::Ptr ptrBlob, std::vector<ROI> &vecROI)
{
    float *ptrFP32 = ptrBlob->buffer().as<float *>();

    size_t outputSize = ptrBlob->byteSize() / sizeof(float);

    float *ptrFP32_ROI = ptrFP32;
    float max = 0.0f;
    float sum = 0.0f;
    int idx = 0;
    for (int j = 0; j < outputSize; j++)
    {
        sum += exp(ptrFP32_ROI[j]);
        if (ptrFP32_ROI[j] > max)
        {
            idx = j;
            max = ptrFP32_ROI[j];
        }
    }
    ROI roi;
    roi.labelIdClassification = idx;
    roi.labelClassification = m_labels.imagenet_labelstring(idx);
    roi.confidenceClassification = exp(max) / sum;
    HVA_DEBUG("roi label is : %s\n", m_labels.imagenet_labelstring(idx).c_str());

    vecROI.clear();
    vecROI.push_back(roi);

    return;
}

void HDDL2pluginHelper_t::postprocGooglenet_fp16(IE::Blob::Ptr ptrBlob, std::vector<ROI> &vecROI)
{
    float *ptrFP32 = ptrBlob->buffer().as<float *>();

    size_t outputSize = ptrBlob->byteSize() / sizeof(float);

    float *ptrFP32_ROI = ptrFP32;
    float max = 0.0f;
    int idx = 0;
    for (int j = 0; j < outputSize; j++)
    {
        if (ptrFP32_ROI[j] > max)
        {
            idx = j;
            max = ptrFP32_ROI[j];
        }
    }
    ROI roi;
    roi.labelIdClassification = idx-1;
    roi.labelClassification = m_labels.imagenet_labelstring(idx-1);
    roi.confidenceClassification = max;
    HVA_DEBUG("roi label is : %s\n", m_labels.imagenet_labelstring(idx-1).c_str());

    vecROI.clear();
    vecROI.push_back(roi);

    return;
}

void HDDL2pluginHelper_t::postprocSqueezenet_fp16(IE::Blob::Ptr ptrBlob, std::vector<ROI> &vecROI)
{
    float *ptrFP32 = ptrBlob->buffer().as<float *>();

    size_t outputSize = ptrBlob->byteSize() / sizeof(float);

    float *ptrFP32_ROI = ptrFP32;
    float max = 0.0f;
    int idx = 0;
    for (int j = 0; j < outputSize; j++)
    {
        if (ptrFP32_ROI[j] > max)
        {
            idx = j;
            max = ptrFP32_ROI[j];
        }
    }
    ROI roi;
    roi.labelIdClassification = idx;
    roi.labelClassification = m_labels.imagenet_labelstring(idx);
    roi.confidenceClassification = max;
    HVA_DEBUG("roi label is : %s\n", m_labels.imagenet_labelstring(idx).c_str());

    vecROI.clear();
    vecROI.push_back(roi);

    return;
}

void HDDL2pluginHelper_t::postprocResnet50_u8(IE::Blob::Ptr ptrBlob, std::vector<ROI> &vecROI)
{
    auto blobDequant = deQuantize(ptrBlob, 0.151837, 67);

    postprocResnet50_fp16(blobDequant, vecROI);

    return;
}

void HDDL2pluginHelper_t::compile(const std::string &graphName)
{
    HVA_DEBUG("compile start : %s\n", graphName.c_str());
    InferenceEngine::Core ie;
    auto network = ie.ReadNetwork(graphName, graphName.substr(0, graphName.length() - 4) + ".bin");

    const auto in_precision = InferenceEngine::Precision::U8;
    for (auto &&layer : network.getInputsInfo())
    {
        layer.second->setLayout(InferenceEngine::Layout::NHWC);
        layer.second->setPrecision(in_precision);
    }

    const auto out_precision = InferenceEngine::Precision::FP16;
    for (auto &&layer : network.getOutputsInfo())
    {
        if (layer.second->getDims().size() == 2)
        {
            layer.second->setLayout(InferenceEngine::Layout::NC);
        }
        else
        {
            layer.second->setLayout(InferenceEngine::Layout::NHWC);
        }
        layer.second->setPrecision(out_precision);
    }
    auto executableNetwork = ie.LoadNetwork(network, "HDDL2", {});

    auto outputName = graphName.substr(0, graphName.length() - 4) + ".blob";
    executableNetwork.Export(outputName);
    HVA_DEBUG("compile end : %s\n", outputName.c_str());
}

IE::Blob::Ptr HDDL2pluginHelper_t::deQuantize(const IE::Blob::Ptr &quantBlob, float scale, uint8_t zeroPoint)
{
    const IE::TensorDesc quantTensor = quantBlob->getTensorDesc();
    const IE::TensorDesc outTensor = IE::TensorDesc(
        IE::Precision::FP32,
        quantTensor.getDims(),
        quantTensor.getLayout());
    const uint8_t *quantRaw = quantBlob->cbuffer().as<const uint8_t *>();

    std::vector<size_t> dims = quantTensor.getDims();

    IE::Blob::Ptr outputBlob = IE::make_shared_blob<float>(outTensor);
    outputBlob->allocate();
    float *outRaw = outputBlob->buffer().as<float *>();
    for (size_t pos = 0; pos < quantBlob->byteSize(); pos++)
    {
        outRaw[pos] = (quantRaw[pos] - zeroPoint) * scale;
    }

    return outputBlob;
}

IE::Blob::Ptr HDDL2pluginHelper_t::yoloLayer_yolov2tiny(const IE::Blob::Ptr &lastBlob, int inputHeight, int inputWidth)
{
    const IE::TensorDesc quantTensor = lastBlob->getTensorDesc();
    const IE::TensorDesc outTensor = IE::TensorDesc(InferenceEngine::Precision::FP32,
                                                    {1, 1, 13 * 13 * 20 * 5, 7},
                                                    lastBlob->getTensorDesc().getLayout());
    IE::Blob::Ptr outputBlob = IE::make_shared_blob<float>(outTensor);
    outputBlob->allocate();
    memset(outputBlob->buffer(), 0, outputBlob->byteSize());
    const float *inputRawData = lastBlob->cbuffer().as<const float *>();
    float *outputRawData = outputBlob->buffer().as<float *>();

    int shape[] = {13, 13, 5, 25};
    int strides[] = {13 * 125, 125, 25, 1};
    postprocess::yolov2(inputRawData, shape, strides, 0.4f, 0.45f, 20, 416, 416, outputRawData);

    return outputBlob;
}

std::vector<std::string> HDDL2pluginHelper_t::readLabelsFromFile(const std::string &labelFileName)
{
    std::vector<std::string> labels;

    std::ifstream inputFile;
    inputFile.open(labelFileName, std::ios::in);
    if (inputFile.is_open())
    {
        std::string strLine;
        while (std::getline(inputFile, strLine))
        {
            labels.push_back(strLine);
        }
    }
    return labels;
}

HDDL2pluginHelper_t::InferRequestPool_t::InferRequestPool_t(IE::ExecutableNetwork &executableNetwork, int32_t numInferRequest)
    : _cntInferRequest{numInferRequest}, _maxSize{static_cast<size_t>(numInferRequest)}
{
    for (int32_t i = 0; i < numInferRequest; i++)
    {
        auto ptrInferRequest = executableNetwork.CreateInferRequestPtr();
        _vecInferRequest.push_back(ptrInferRequest);
        _queue.push(ptrInferRequest);
    }
}
HDDL2pluginHelper_t::InferRequestPool_t::~InferRequestPool_t()
{
    for (auto ptrInferRequest : _vecInferRequest)
    {
        ptrInferRequest->Wait(1000000);
        ptrInferRequest->SetCompletionCallback([] {});
    }
    while (_queue.size() != _vecInferRequest.size())
    {
        usleep(10000);
    }
}

HDDL2pluginHelper_t::InferRequestPool_t::Type HDDL2pluginHelper_t::InferRequestPool_t::get()
{
    Type ptr = nullptr;
    pop(ptr);
    return ptr;
}

void HDDL2pluginHelper_t::InferRequestPool_t::put(const HDDL2pluginHelper_t::InferRequestPool_t::Type ptrInferRequest)
{
    push(ptrInferRequest);
}

bool HDDL2pluginHelper_t::InferRequestPool_t::push(HDDL2pluginHelper_t::InferRequestPool_t::Type value)
{
    std::unique_lock<std::mutex> lk(_mutex);
    _cv_full.wait(lk, [this] { return (_queue.size() <= _maxSize || true == _close); });
    if (true == _close)
    {
        return false;
    }
    _queue.push(std::move(value));
    _cv_empty.notify_one();
    return true;
}

bool HDDL2pluginHelper_t::InferRequestPool_t::pop(HDDL2pluginHelper_t::InferRequestPool_t::Type &value)
{
    std::unique_lock<std::mutex> lk(_mutex);
    _cv_empty.wait(lk, [this] { return (!_queue.empty() || true == _close); });
    if (true == _close)
    {
        return false;
    }
    value = _queue.front();
    _queue.pop();
    _cv_full.notify_one();
    return true;
}

bool HDDL2pluginHelper_t::InferRequestPool_t::empty() const
{
    std::lock_guard<std::mutex> lk(_mutex);
    return _queue.empty();
}

void HDDL2pluginHelper_t::InferRequestPool_t::close()
{
    std::lock_guard<std::mutex> lk(_mutex);
    _close = true;
    _cv_empty.notify_all();
    _cv_full.notify_all();
    return;
}


void HDDL2pluginHelper_t::OrderKeeper_t::lock(uint64_t id)
{
    std::unique_lock<std::mutex> lock(_mutex);
    HVA_DEBUG("order keeper lock\n");
    if (id < _cntOrder)
    {
        HVA_DEBUG("[warning] duplicated order id\n");
    }
    _cv.wait(lock, [&] { return id <= _cntOrder; });
}

void HDDL2pluginHelper_t::OrderKeeper_t::unlock(uint64_t id)
{
    std::unique_lock<std::mutex> lock(_mutex);
    HVA_DEBUG("order keeper unlock\n");

    if (id != _cntOrder)
    {
        throw std::logic_error("unlock & lock must be called in pair");
    }

    _cntOrder++;

    while (_setFrameId.find(_cntOrder) != _setFrameId.end())
    {
        _setFrameId.erase(_cntOrder);
        _cntOrder++;
    }
    _cv.notify_all();
}

void HDDL2pluginHelper_t::OrderKeeper_t::bypass(uint64_t id)
{
    std::unique_lock<std::mutex> lock(_mutex);
    HVA_DEBUG("order keeper bypass\n");

    if (id <= _cntOrder)
    {
        if (id < _cntOrder)
        {
            HVA_WARNING("duplicated order id\n");
        }
        else // id == _cntOrder
        {
            _cntOrder++;

            while (_setFrameId.find(_cntOrder) != _setFrameId.end())
            {
                _setFrameId.erase(_cntOrder);
                _cntOrder++;
            }

            _cv.notify_all();
        }
    }
    else
    {
        _setFrameId.insert(id);
    }
}
