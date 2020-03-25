#ifndef HDDL2PLUGIN_HELPER_HPP
#define HDDL2PLUGIN_HELPER_HPP

#include <iostream>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <fstream>
#include <memory>

#include <inference_engine.hpp>
#include <ie_compound_blob.h>

#include <opencv2/opencv.hpp>
#include "WorkloadContext.h"
#include <HddlUnite.h>
#include <RemoteMemory.h>
#include <hddl2_params.hpp>

#include "ie_compound_blob.h"
#include "tinyYolov2_post.h"
#include "region_yolov2tiny.h"
#include "detection_helper.hpp"

#include <fstream>

// using namespace InferenceEngine;
namespace IE = InferenceEngine;

class HDDL2pluginHelper_t
{
public:
    using PostprocPtr_t = std::function<void(HDDL2pluginHelper_t*)>;

    HDDL2pluginHelper_t() = default;
    ~HDDL2pluginHelper_t() = default;
    HDDL2pluginHelper_t(const HDDL2pluginHelper_t &) = delete;
    HDDL2pluginHelper_t &operator=(const HDDL2pluginHelper_t &) = delete;

    HDDL2pluginHelper_t(std::string graphPath, WorkloadID id = 0, size_t heightInput = 0, size_t widthInput = 0, PostprocPtr_t ptrPostproc = nullptr)
        : _graphPath{graphPath},
          _workloadId{id},
          _heightInput{heightInput},
          _widthInput{widthInput},
          _ptrPostproc{ptrPostproc}
    {
    }

    inline void setup()
    {
        // _graphPath = "/home/kmb/cong/graph/resnet-50-dpu/resnet-50-dpu.blob";

        // _graphPath = "/home/kmb/cong/fp16_test/resnet/resnet50_uint8_int8_weights_pertensor.blob";
        // _inputPath = "/home/kmb/cong/fp16_test/resnet/input.bin";
        // outputPath = "./output.bin";

//todo, no need to create unite context
#if 1 
        printf("[debug] start create context\n");
        _ptrContext = HddlUnite::createWorkloadContext();
        assert(nullptr != _ptrContext.get());

        _ptrContext->setContext(_workloadId);
        assert(_workloadId == _ptrContext->getWorkloadContextID());
        assert(HddlStatusCode::HDDL_OK == HddlUnite::registerWorkloadContext(_ptrContext));
        printf("[debug] end create context\n");
#endif //todo, no need to create unite context

        // ---- Load inference engine instance
        printf("[debug] start load graph\n");

        // ---- Init context map and create context based on it
        IE::ParamMap paramMap = {{IE::HDDL2_PARAM_KEY(WORKLOAD_CONTEXT_ID), _workloadId}};
        _ptrContextIE = _ie.CreateContext("HDDL2", paramMap);

        // ---- Import network providing context as input to bind to context
        const std::string &modelPath = _graphPath;

        std::filebuf blobFile;
        if (!blobFile.open(modelPath, std::ios::in | std::ios::binary))
        {
            blobFile.close();
            THROW_IE_EXCEPTION << "Could not open file: " << modelPath;
        }
        std::istream graphBlob(&blobFile);

        _executableNetwork = _ie.ImportNetwork(graphBlob, _ptrContextIE);
        printf("[debug] end load graph\n");

        // ---- Create infer request
        _inferRequest = _executableNetwork.CreateInferRequest();
    }

    inline void update(int fd = 0, size_t heightInput = 0, size_t widthInput = 0, const std::vector<InfoROI_t> &vecROI = std::vector<InfoROI_t>())
    {
//todo, no need to load data from file
#if 1 
        _inputPath = "/home/kmb/cong/graph/resnet-50-dpu/input.bin";
        _inputPath = "/home/kmb/cong/graph/resnet-50-dpu/input-cat-1080x1080-nv12.bin";

        // ---- Load frame to remote memory (emulate VAAPI result)
        // ----- Load binary input
        printf("[debug] start create input tensor\n");

        const auto &inputTensor = InferenceEngine::TensorDesc(InferenceEngine::Precision::U8,
                                                              {1, 1, 1, _heightInput * _widthInput * 3 / 2},
                                                              InferenceEngine::Layout::NCHW);
        auto inputRefBlob = IE::make_shared_blob<uint8_t>(inputTensor);
        inputRefBlob->allocate();

        // (vpu::KmbPlugin::utils::fromBinaryFile(_inputPath, inputRefBlob));
        FILE *fp;

        printf("inputRefBlob->byteSize() is :%d\n", inputRefBlob->byteSize());
        fp = fopen(_inputPath.c_str(), "rb");
        fread(inputRefBlob->buffer().as<void *>(), inputRefBlob->byteSize(), 1, fp);
        fclose(fp);

        fp = fopen("originalInput.bin", "wb");
        fwrite(inputRefBlob->buffer().as<void *>(), inputRefBlob->byteSize(), 1, fp);
        fclose(fp);

        // ----- Allocate memory with HddlUnite on device
#if 0
        _remoteMemoryFd =
            allocateRemoteMemory(_ptrContext, inputRefBlob->buffer().as<void*>(), inputRefBlob->byteSize());
#else
        _remoteFrame = HddlUnite::SMM::allocate(*_ptrContext, inputRefBlob->byteSize());

        if (_remoteFrame == nullptr)
        {
            THROW_IE_EXCEPTION << "Failed to allocate remote memory.";
        }

        if (_remoteFrame->syncToDevice(inputRefBlob->buffer().as<void *>(), inputRefBlob->byteSize()) != HDDL_OK)
        {
            THROW_IE_EXCEPTION << "Failed to sync memory to device.";
        }
        _remoteMemoryFd = _remoteFrame->getDmaBufFd();
#endif

        HddlUnite::SMM::RemoteMemory memTemp(*_ptrContext, _remoteMemoryFd, inputRefBlob->byteSize());

        char *buf = (char *)malloc(inputRefBlob->byteSize());
        memTemp.syncFromDevice(buf, inputRefBlob->byteSize());

        cv::Mat matTemp{_heightInput * 3 / 2, _widthInput, CV_8UC1, buf};
        cv::cvtColor(matTemp, _frameBGR, cv::COLOR_YUV2BGR_NV12);

        // std::ofstream fileRead("./readInput.bin", std::ios::out | std::ios::binary);
        // fileRead.write(buf, inputRefBlob->byteSize());
        fp = fopen("remoteInput.bin", "wb");
        fwrite(buf, 1, inputRefBlob->byteSize(), fp);
        fclose(fp);

        free(buf);
#endif //todo, no need to load data from file

        if (fd > 0)
        {
            _remoteMemoryFd = fd;
        }
        if (heightInput > 0)
        {
            _heightInput = heightInput;
        }
        if (widthInput > 0)
        {
            _widthInput = widthInput;
        }

        // ---- Create remote blob by using already exists fd
        assert(_remoteMemoryFd >= 0);
#if 0
        IE::ParamMap blobParamMap = {{IE::HDDL2_PARAM_KEY(REMOTE_MEMORY_FD), static_cast<uint64_t>(_remoteMemoryFd)}};
#else
        IE::ParamMap blobParamMap = {{IE::HDDL2_PARAM_KEY(REMOTE_MEMORY_FD), static_cast<uint64_t>(_remoteMemoryFd)},
                                     {IE::HDDL2_PARAM_KEY(COLOR_FORMAT), IE::ColorFormat::NV12}};
#endif
        auto inputsInfo = _executableNetwork.GetInputsInfo();
        _inputName = _executableNetwork.GetInputsInfo().begin()->first;

#if 0
        IE::InputInfo::CPtr inputInfoPtr = _executableNetwork.GetInputsInfo().begin()->second;
        _ptrRemoteBlob = _ptrContextIE->CreateBlob(inputInfoPtr->getTensorDesc(), blobParamMap);
#else
        IE::TensorDesc inputTensorDesc = IE::TensorDesc(IE::Precision::U8, {1, 3, _heightInput, _widthInput}, IE::Layout::NCHW);
        // IE::TensorDesc inputTensorDescTemp = _executableNetwork.GetInputsInfo().begin()->second->getTensorDesc();
        // assert(inputTensorDesc == inputTensorDescTemp);
        _ptrRemoteBlob = _ptrContextIE->CreateBlob(inputTensorDesc, blobParamMap);
#endif
        assert(nullptr != _ptrRemoteBlob);

        printf("[debug] start dump input\n");
        std::ofstream fileInput("./input.bin", std::ios_base::out | std::ios_base::binary);

        const size_t inputSize = _ptrRemoteBlob->byteSize();
        fileInput.write(_ptrRemoteBlob->buffer(), inputSize);
        printf("[debug] end dump input\n");

#if 0
        // ---- Set remote blob as input for infer request         
        _inferRequest.SetBlob(_inputName, _ptrRemoteBlob);
#else
        // Since it 228x228 image on 224x224 network, resize preprocessing also required
        IE::PreProcessInfo preprocInfo = _inferRequest.GetPreProcess(_inputName);
        preprocInfo.setResizeAlgorithm(IE::RESIZE_BILINEAR);
        preprocInfo.setColorFormat(IE::ColorFormat::NV12);

        // ---- Set remote NV12 blob with preprocessing information
        _inferRequest.SetBlob(_inputName, _ptrRemoteBlob, preprocInfo);
#endif
        printf("[debug] end create input tensor\n");
    }

    inline void infer()
    {
        std::string outputPath = "./output.bin";

        // ---- Run the request synchronously
        printf("[debug] start inference\n");
        _inferRequest.Infer();
        printf("[debug] end inference\n");

        // --- Get output
        printf("[debug] start dump output file\n");
        auto outputBlobName = _executableNetwork.GetOutputsInfo().begin()->first;
        _ptrOutputBlob = _inferRequest.GetBlob(outputBlobName);

        std::ofstream file(outputPath, std::ios_base::out | std::ios_base::binary);
        if (!file.good() || !file.is_open())
        {
            std::stringstream err;
            err << "Cannot access output file. File path: " << outputPath;
            throw std::runtime_error(err.str());
        }

        const size_t outputSize = _ptrOutputBlob->byteSize();
        file.write(_ptrOutputBlob->buffer(), outputSize);
        printf("[debug] end dump output file\n");

        assert(_ptrOutputBlob->getTensorDesc().getPrecision() == IE::Precision::U8);
    }

    inline void postproc()
    {
        if (nullptr != _ptrPostproc)
        {
            _ptrPostproc(this);
        }
    }

    //todo, fix me
    inline void postprocYolotinyv2_fp16()
    {
        std::vector<DetectedObject_t> &vecObjects = _vecObjects;
        //todo, fix me
#if 1
        vecObjects = ::YoloV2Tiny::TensorToBBoxYoloV2TinyCommon(_ptrOutputBlob, _heightInput, _widthInput,
                                                                .6, YoloV2Tiny::fillRawNetOut);
#else
        auto blobDequant = deQuantize(_ptrOutputBlob, 0.271045, 210);
        // auto desc = output_blob->getTensorDesc();
        // auto dims = desc.getDims();
        // float *data = static_cast<float *>(output_blob->buffer());
        // Blob::Ptr dequantOut = InferNodeWorker::deQuantize(output_blob, 0.33713474f, 221);

        // FILE* fp = fopen("dumpOutput.bin", "wb");
        // fwrite((unsigned char*)data, 1, output_blob->byteSize(), fp);
        // fclose(fp);

        // Region YOLO layer
        // const int imageWidth = 1080;
        // const int imageHeight = 720;
        IE::Blob::Ptr detectResult = yoloLayer_yolov2tiny(blobDequant, _heightInput, _widthInput);

        // std::vector<DetectedObject_t> vecObjects;
        vecObjects.clear();

        // Print result.
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
                object.x = (rawData[i * 7 + 3]) * _widthInput / 416;
                object.y = (rawData[i * 7 + 4]) * _heightInput / 416;
                object.width = (rawData[i * 7 + 5] - rawData[i * 7 + 3]) * _widthInput / 416;
                object.height = (rawData[i * 7 + 6] - rawData[i * 7 + 4]) * _heightInput / 416;
                object.confidence = rawData[i * 7 + 2];
                vecObjects.push_back(object);

                std::cout << "confidence = " << rawData[i * 7 + 2] << std::endl;
                std::cout << "x0,y0,x1,y1 = " << rawData[i * 7 + 3] << ", "
                          << rawData[i * 7 + 4] << ", "
                          << rawData[i * 7 + 5] << ", "
                          << rawData[i * 7 + 6] << std::endl;
            }
        }
#endif
        for (auto &object : vecObjects)
        {
            printf("[debug] object detected: x is %d, y is %d, w is %d, h is %d\n", object.x, object.y, object.width, object.height);
            cv::rectangle(_frameBGR, cv::Rect(object.x, object.y, object.width, object.height), cv::Scalar(0, 255, 0), 2);
        }
        char filename[256];
        snprintf(filename, _inputPath.size() - 4, "%s", _inputPath.c_str());
        snprintf(filename + strlen(filename), 256, "%s", "output.jpg");
        static int frameCnt = 0;
        snprintf(filename, 256, "./output/debug-output-%d.jpg", frameCnt);
        frameCnt++;
        std::cout << filename << std::endl;
        // cv::imwrite(filename, frameBGR);
        // cv::imshow("output", frameBGR);
        // cv::waitKey(10);
    }

    inline void postprocYolotinyv2_u8()
    {
        std::vector<DetectedObject_t> &vecObjects = _vecObjects;
        //todo, fix me
#if 0
        vecObjects = ::YoloV2Tiny::TensorToBBoxYoloV2TinyCommon(_ptrOutputBlob, _heightInput, _widthInput,
                                                               .6, YoloV2Tiny::fillRawNetOut);
#else
        auto blobDequant = deQuantize(_ptrOutputBlob, 0.271045, 210);
        // auto desc = output_blob->getTensorDesc();
        // auto dims = desc.getDims();
        // float *data = static_cast<float *>(output_blob->buffer());
        // Blob::Ptr dequantOut = InferNodeWorker::deQuantize(output_blob, 0.33713474f, 221);

        // FILE* fp = fopen("dumpOutput.bin", "wb");
        // fwrite((unsigned char*)data, 1, output_blob->byteSize(), fp);
        // fclose(fp);

        // Region YOLO layer
        // const int imageWidth = 1080;
        // const int imageHeight = 720;
        IE::Blob::Ptr detectResult = yoloLayer_yolov2tiny(blobDequant, _heightInput, _widthInput);

        // std::vector<DetectedObject_t> vecObjects;
        vecObjects.clear();

        // Print result.
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
                object.x = (rawData[i * 7 + 3]) * _widthInput / 416;
                object.y = (rawData[i * 7 + 4]) * _heightInput / 416;
                object.width = (rawData[i * 7 + 5] - rawData[i * 7 + 3]) * _widthInput / 416;
                object.height = (rawData[i * 7 + 6] - rawData[i * 7 + 4]) * _heightInput / 416;
                object.confidence = rawData[i * 7 + 2];
                vecObjects.push_back(object);

                std::cout << "confidence = " << rawData[i * 7 + 2] << std::endl;
                std::cout << "x0,y0,x1,y1 = " << rawData[i * 7 + 3] << ", "
                          << rawData[i * 7 + 4] << ", "
                          << rawData[i * 7 + 5] << ", "
                          << rawData[i * 7 + 6] << std::endl;
            }
        }
#endif
        for (auto &object : vecObjects)
        {
            printf("[debug] object detected: x is %d, y is %d, w is %d, h is %d\n", object.x, object.y, object.width, object.height);
            cv::rectangle(_frameBGR, cv::Rect(object.x, object.y, object.width, object.height), cv::Scalar(0, 255, 0), 2);
        }
        char filename[256];
        snprintf(filename, _inputPath.size() - 4, "%s", _inputPath.c_str());
        snprintf(filename + strlen(filename), 256, "%s", "output.jpg");
        static int frameCnt = 0;
        snprintf(filename, 256, "./output/debug-output-%d.jpg", frameCnt);
        frameCnt++;
        std::cout << filename << std::endl;
        cv::imwrite(filename, _frameBGR);
        // cv::imshow("output", frameBGR);
        // cv::waitKey(10);
    }

    //todo, fix me
    inline void postprocResnet50_fp16()
    {
        //todo, fix me
        // std::vector<int>& vecIdx = _vecIdx;
        // std::vector<std::string>& vecLabel = _vecLabel;
        // std::vector<float>& vecConfidence = _vecConfidence;
        // vecIdx.clear();
        // vecLabel.clear();
        // vecConfidence.clear();

        size_t outputSize = 2000;
        for (int i = 0; i < std::min(_vecROI.size(), 10ul); i++)
        {
            //todo, fix me
            // float *ptrFP32_ROI = ptrFP32 + outputSize / 2 * i;
            // float max = 0.0f;
            // float sum = 0.0f;
            // int idx = 0;
            // for (int j = 0; j < outputSize / 2; j++)
            // {
            //     sum += exp(ptrFP32_ROI[j]);
            //     if (ptrFP32_ROI[j] > max)
            //     {
            //         idx = j;
            //         max = ptrFP32_ROI[j];
            //     }
            // }
            // _vecROI[i].idx = idx;
            // std::vector<std::string> labels = readLabelsFromFile("/home/kmb/cong/graph/resnet.labels");
            // _vecROI[i].label = labels[idx];
            // _vecROI[i].confidence = exp(max) / sum;
            // printf("[debug] roi label is : %s\n", labels[idx].c_str());
        }
    }

    inline void postprocResnet50_u8()
    {
        //todo, fix me
        // std::vector<int>& vecIdx = _vecIdx;
        // std::vector<std::string>& vecLabel = _vecLabel;
        // std::vector<float>& vecConfidence = _vecConfidence;
        // vecIdx.clear();
        // vecLabel.clear();
        // vecConfidence.clear();

        auto blobDequant = deQuantize(_ptrOutputBlob, 0.271045, 210);
        size_t outputSize = 1000;
        
        //todo, fix me single roi
        if (_vecROI.size() > 0)
        {
            float *ptrFP32_ROI = blobDequant->buffer().as<float*>() + outputSize;
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
            _vecROI[0].idx = idx;
            std::vector<std::string> labels = readLabelsFromFile("/home/kmb/cong/graph/resnet.labels");
            _vecROI[0].label = labels[idx];
            _vecROI[0].confidence = exp(max) / sum;
            printf("[debug] roi label is : %s\n", labels[idx].c_str());
        }
    }

private:
    IE::Blob::Ptr deQuantize(const IE::Blob::Ptr &quantBlob, float scale, uint8_t zeroPoint)
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
        float *outRaw = outputBlob->buffer().as<float*>();
        for (size_t pos = 0; pos < quantBlob->byteSize(); pos++)
        {
            outRaw[pos] = (quantRaw[pos] - zeroPoint) * scale;
        }

        return outputBlob;
    }

    IE::Blob::Ptr yoloLayer_yolov2tiny(const IE::Blob::Ptr &lastBlob, int inputHeight, int inputWidth)
    {
        const IE::TensorDesc quantTensor = lastBlob->getTensorDesc();
        const IE::TensorDesc outTensor = IE::TensorDesc(InferenceEngine::Precision::FP32,
                                                        {1, 1, 13 * 13 * 20 * 5, 7},
                                                        lastBlob->getTensorDesc().getLayout());
        IE::Blob::Ptr outputBlob = IE::make_shared_blob<float>(outTensor);
        outputBlob->allocate();
        memset(outputBlob->buffer(), 0, outputBlob->byteSize());
        const float *inputRawData = lastBlob->cbuffer().as<const float *>();
        float *outputRawData = outputBlob->buffer().as<float*>();

        int shape[] = {13, 13, 5, 25};
        int strides[] = {13 * 125, 125, 25, 1};
        postprocess::yolov2(inputRawData, shape, strides, 0.4f, 0.45f, 20, 416, 416, outputRawData);

        return outputBlob;
    }

    std::vector<std::string> readLabelsFromFile(const std::string &labelFileName)
    {
        std::vector<std::string> labels;

        std::ifstream inputFile;
        inputFile.open(labelFileName, std::ios::in);
        if (inputFile.is_open())
        {
            std::string strLine;
            while (std::getline(inputFile, strLine))
            {
                // trim(strLine);
                labels.push_back(strLine);
            }
        }
        return labels;
    }

private:
    IE::Core _ie;
    IE::RemoteContext::Ptr _ptrContextIE;
    IE::ExecutableNetwork _executableNetwork;
    IE::InferRequest _inferRequest;
    IE::RemoteBlob::Ptr _ptrRemoteBlob;
    IE::Blob::Ptr _ptrOutputBlob;
    std::string _inputName{""};

    HddlUnite::WorkloadContext::Ptr _ptrContext;
    WorkloadID _workloadId{0ul};
    int _remoteMemoryFd{0};

    std::string _graphPath{""};
    size_t _heightInput{0};
    size_t _widthInput{0};

    PostprocPtr_t _ptrPostproc{nullptr};

public:
    std::vector<DetectedObject_t> _vecObjects;
    std::vector<InfoROI_t> _vecROI;

    //todo, only for test
    cv::Mat _frameBGR;
    HddlUnite::SMM::RemoteMemory::Ptr _remoteFrame;
    std::string _inputPath{""};
};

#endif //#ifndef HDDL2PLUGIN_HELPER_HPP