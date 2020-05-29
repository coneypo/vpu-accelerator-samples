#ifndef HDDL2PLUGIN_HELPER_HPP
#define HDDL2PLUGIN_HELPER_HPP

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

#include <opencv2/opencv.hpp>
#include <WorkloadContext.h>
#include <HddlUnite.h>
#include <RemoteMemory.h>

#include "hvaLogger.hpp"
#include "hddl2plugin/hddl2_params.hpp"

#include "common.hpp"
#include "tinyYolov2_post.h"
#include "region_yolov2tiny.h"
#include "detection_helper.hpp"
#include "ImageNetLabels.hpp"
#include "hvaLogger.hpp"


#define HDDLPLUGIN_PROFILE
#define INFER_ROI
#define SINGLE_ROI_POSTPROC
#define INFER_FP16


const int32_t INFER_REQUEST_NUM = 1;

// using namespace InferenceEngine;
namespace IE = InferenceEngine;

class HDDL2pluginHelper_t
{
public:
    using PostprocPtr_t = std::function<void(HDDL2pluginHelper_t*, IE::Blob::Ptr, std::vector<ROI>& vecROI)>;
    using InferCallback_t = std::function<void(void)>;

    HDDL2pluginHelper_t() = default;
    ~HDDL2pluginHelper_t() = default;
    HDDL2pluginHelper_t(const HDDL2pluginHelper_t &) = delete;
    HDDL2pluginHelper_t &operator=(const HDDL2pluginHelper_t &) = delete;

    HDDL2pluginHelper_t(std::string graphPath, WorkloadID id = 0, PostprocPtr_t ptrPostproc = nullptr, float thresholdDetection = 0.6f, size_t heightInput = 0, 
                        size_t widthInput = 0)
        : _graphPath{graphPath},
          _workloadId{id},
          _heightInput{heightInput},
          _widthInput{widthInput},
          _ptrPostproc{ptrPostproc},
          _thresholdDetection{thresholdDetection}
    {
    }

    inline void setup(int32_t numInferRequest = 1)
    {
        // _graphPath = "/home/kmb/cong/graph/resnet-50-dpu/resnet-50-dpu.blob";

        // _graphPath = "/home/kmb/cong/fp16_test/resnet/resnet50_uint8_int8_weights_pertensor.blob";
        // _inputPath = "/home/kmb/cong/fp16_test/resnet/input.bin";
        // outputPath = "./output.bin";

//todo, no need to create unite context
#if 0
        HVA_DEBUG("[debug] start create context\n");
        _ptrContext = HddlUnite::createWorkloadContext();
        assert(nullptr != _ptrContext.get());

        _ptrContext->setContext(_workloadId);
        assert(_workloadId == _ptrContext->getWorkloadContextID());
        assert(HddlStatusCode::HDDL_OK == HddlUnite::registerWorkloadContext(_ptrContext));
        HVA_DEBUG("[debug] end create context\n");
#endif //todo, no need to create unite context

        // ---- Load inference engine instance
        HVA_DEBUG("[debug] start load graph\n");

        // ---- Init context map and create context based on it
        IE::ParamMap paramMap = {{IE::HDDL2_PARAM_KEY(WORKLOAD_CONTEXT_ID), _workloadId}};
        _ptrContextIE = _ie.CreateContext("HDDL2", paramMap);

        // ---- Import network providing context as input to bind to context
        const std::string &modelPath = _graphPath;

        if(_graphPath.find(".blob") != std::string::npos)
        {
            std::filebuf blobFile;
            if (!blobFile.open(modelPath, std::ios::in | std::ios::binary))
            {
                blobFile.close();
                THROW_IE_EXCEPTION << "Could not open file: " << modelPath;
            }
            std::istream graphBlob(&blobFile);
            _executableNetwork = _ie.ImportNetwork(graphBlob, _ptrContextIE);
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
            
            _executableNetwork = _ie.LoadNetwork(network, _ptrContextIE, {});
        }
        HVA_DEBUG("[debug] end load graph\n");

        // ---- Create infer request
        //todo, fix me
        // _ptrInferRequest = _executableNetwork.CreateInferRequestPtr();
        _ptrInferRequestPool = std::make_shared<InferRequestPool_t> (_executableNetwork, numInferRequest);
    }

	inline void setupImgPipe(int32_t numInferRequest = 1) {
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

            _executableNetwork = _ie.LoadNetwork(network, "HDDL2");
        }
//		_executableNetwork = _ie.ImportNetwork(_graphPath, "HDDL2");
		//        printf("[debug] end load graph\n");

		// ---- Create infer request
		_ptrInferRequest = _executableNetwork.CreateInferRequestPtr();
		_ptrInferRequestPool = std::make_shared<InferRequestPool_t>(
				_executableNetwork, numInferRequest);
	}

    //todo fix me
    inline void update(int fd = 0, size_t heightInput = 0, size_t widthInput = 0, const std::vector<ROI> &vecROI = std::vector<ROI>())
    {
//todo, no need to load data from file
#if 0
        _inputPath = "/home/kmb/cong/graph/resnet-50-dpu/input.bin";
        _inputPath = "/home/kmb/cong/graph/resnet-50-dpu/input-cat-1080x1080-nv12.bin";

        // ---- Load frame to remote memory (emulate VAAPI result)
        // ----- Load binary input
        HVA_DEBUG("[debug] start create input tensor\n");

        const auto &inputTensor = InferenceEngine::TensorDesc(InferenceEngine::Precision::U8,
                                                              {1, 1, 1, _heightInput * _widthInput * 3 / 2},
                                                              InferenceEngine::Layout::NCHW);
        auto inputRefBlob = IE::make_shared_blob<uint8_t>(inputTensor);
        inputRefBlob->allocate();

        // (vpu::KmbPlugin::utils::fromBinaryFile(_inputPath, inputRefBlob));
        FILE *fp;

        HVA_DEBUG("inputRefBlob->byteSize() is :%d\n", inputRefBlob->byteSize());
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

//todo, only for test
#if 0
        _ptrContext = HddlUnite::queryWorkloadContext(_workloadId);
        HddlUnite::SMM::RemoteMemory memTemp(*_ptrContext, _remoteMemoryFd, _heightInput * _widthInput * 3 / 2);

        char *buf = (char *)malloc(_heightInput * _widthInput * 3 / 2);
        memTemp.syncFromDevice(buf, _heightInput * _widthInput * 3 / 2);

        cv::Mat matTemp{_heightInput * 3 / 2, _widthInput, CV_8UC1, buf};
        cv::cvtColor(matTemp, _frameBGR, cv::COLOR_YUV2BGR_NV12);

        cv::imwrite("./input-nv12-cvt.jpg", _frameBGR);
#endif
        // ---- Create remote blob by using already exists fd
        assert(_remoteMemoryFd >= 0);

#ifdef HDDLPLUGIN_PROFILE        
        auto start = std::chrono::steady_clock::now();
#endif

#if 0
        IE::ParamMap blobParamMap = {{IE::HDDL2_PARAM_KEY(REMOTE_MEMORY_FD), static_cast<uint64_t>(_remoteMemoryFd)}};
#else

#ifndef INFER_ROI
        IE::ParamMap blobParamMap = {{IE::HDDL2_PARAM_KEY(REMOTE_MEMORY_FD), static_cast<uint64_t>(_remoteMemoryFd)},
                                     {IE::HDDL2_PARAM_KEY(COLOR_FORMAT), IE::ColorFormat::NV12}};
#else
        IE::ROI roiIE{0, 0, 0, widthInput, heightInput};
        IE::ParamMap blobParamMap = {{IE::HDDL2_PARAM_KEY(REMOTE_MEMORY_FD), static_cast<uint64_t>(_remoteMemoryFd)},
                                     {IE::HDDL2_PARAM_KEY(COLOR_FORMAT), IE::ColorFormat::NV12},
                                     {IE::HDDL2_PARAM_KEY(ROI), roiIE}};
#endif

#endif

        //todo fix me
        if (_executableNetwork.GetInputsInfo().empty())
        {
            return;
        }

        auto inputsInfo = _executableNetwork.GetInputsInfo();
        _inputName = _executableNetwork.GetInputsInfo().begin()->first;
        
#ifdef HDDLPLUGIN_PROFILE  
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        HVA_DEBUG("[debug] inputsInfo duration is %ld, mode is %s\n", duration, _graphPath.c_str());

        start = std::chrono::steady_clock::now();
#endif

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

#ifdef HDDLPLUGIN_PROFILE
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        HVA_DEBUG("[debug] CreateBlob duration is %ld, mode is %s\n", duration, _graphPath.c_str());
#endif

#if 0//todo, only for debug
        HVA_DEBUG("[debug] start dump input\n");
        std::ofstream fileInput("./input.bin", std::ios_base::out | std::ios_base::binary);

        const size_t inputSize = _ptrRemoteBlob->byteSize();
        fileInput.write(_ptrRemoteBlob->buffer(), inputSize);
        HVA_DEBUG("[debug] end dump input\n");
#endif

#ifdef HDDLPLUGIN_PROFILE  
        start = std::chrono::steady_clock::now();
#endif

#if 0
        // ---- Set remote blob as input for infer request         
        _ptrInferRequest->SetBlob(_inputName, _ptrRemoteBlob);
#else
        // Since it 228x228 image on 224x224 network, resize preprocessing also required
        IE::PreProcessInfo preprocInfo = _ptrInferRequest->GetPreProcess(_inputName);
        preprocInfo.setResizeAlgorithm(IE::RESIZE_BILINEAR);
        preprocInfo.setColorFormat(IE::ColorFormat::NV12);

        // ---- Set remote NV12 blob with preprocessing information
        _ptrInferRequest->SetBlob(_inputName, _ptrRemoteBlob, preprocInfo);    
#endif

#ifdef INFER_FP16

        auto outputBlobName = _executableNetwork.GetOutputsInfo().begin()->first;
        auto outputBlob = _ptrInferRequest->GetBlob(outputBlobName);
        auto& desc = outputBlob->getTensorDesc();

        desc.setPrecision(IE::Precision::FP32);  
#endif

#ifdef HDDLPLUGIN_PROFILE  
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        HVA_DEBUG("[debug] SetBlob duration is %ld, mode is %s\n", duration, _graphPath.c_str());
#endif

        HVA_DEBUG("[debug] end create input tensor\n");
    }
    //todo fix me
    inline IE::Blob::Ptr infer()
    {
        // std::string outputPath = "./output.bin";

        // ---- Run the request synchronously
        HVA_DEBUG("[debug] start inference\n");

#ifdef HDDLPLUGIN_PROFILE        
        auto start = std::chrono::steady_clock::now();
#endif
        _ptrInferRequest->Infer();
#ifdef HDDLPLUGIN_PROFILE  
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        HVA_DEBUG("[debug] hddl2plugin infer duration is %ld, mode is %s\n", duration, _graphPath.c_str());
#endif
        HVA_DEBUG("[debug] end inference\n");

        // --- Get output
        HVA_DEBUG("[debug] start dump output file\n");

#ifdef HDDLPLUGIN_PROFILE
        start = std::chrono::steady_clock::now();
#endif
        //todo fix me
        if (_executableNetwork.GetOutputsInfo().empty())
        {
            return nullptr;
        }

        auto outputBlobName = _executableNetwork.GetOutputsInfo().begin()->first;
        auto ptrOutputBlob = _ptrInferRequest->GetBlob(outputBlobName);
#ifdef HDDLPLUGIN_PROFILE  
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        HVA_DEBUG("[debug] GetBlob duration is %ld, mode is %s\n", duration, _graphPath.c_str());
#endif

#if 0 //only for debug
        std::ofstream file(outputPath, std::ios_base::out | std::ios_base::binary);
        if (!file.good() || !file.is_open())
        {
            std::stringstream err;
            err << "Cannot access output file. File path: " << outputPath;
            throw std::runtime_error(err.str());
        }

        const size_t outputSize = ptrOutputBlob->byteSize();
        file.write(ptrOutputBlob->buffer(), outputSize);
#endif
        HVA_DEBUG("[debug] end dump output file\n");

        return ptrOutputBlob;
    }

    inline void inferAsync(IE::InferRequest::Ptr ptrInferRequest, InferCallback_t callback, 
                int remoteMemoryFd = 0, size_t heightInput = 0, size_t widthInput = 0, const ROI &roi = ROI{})
    {
        //todo, fix me
        if (0 == remoteMemoryFd)
        {
            remoteMemoryFd = _remoteMemoryFd;
        }
        if (0 == heightInput)
        {
            heightInput = _heightInput;
        }
        else
        {
            _heightInput = heightInput;
        }
        if (0 == widthInput)
        {
            widthInput = _widthInput;
        }
        else
        {
            _widthInput = widthInput;
        }

//todo, only for test

        // ---- Create remote blob by using already exists fd
        assert(remoteMemoryFd >= 0);

#ifdef HDDLPLUGIN_PROFILE        
        auto start = std::chrono::steady_clock::now();
#endif

#ifndef INFER_ROI
        IE::ParamMap blobParamMap = {{IE::HDDL2_PARAM_KEY(REMOTE_MEMORY_FD), static_cast<uint64_t>(remoteMemoryFd)},
                                     {IE::HDDL2_PARAM_KEY(COLOR_FORMAT), IE::ColorFormat::NV12}};
#else
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
        IE::ParamMap blobParamMap = {{IE::HDDL2_PARAM_KEY(REMOTE_MEMORY_FD), static_cast<uint64_t>(remoteMemoryFd)},
                                     {IE::HDDL2_PARAM_KEY(COLOR_FORMAT), IE::ColorFormat::NV12},
                                     {IE::HDDL2_PARAM_KEY(ROI), roiIE}};
#endif

        //todo fix me
        if (_executableNetwork.GetInputsInfo().empty())
        {
            return;
        }

        auto inputsInfo = _executableNetwork.GetInputsInfo();
        _inputName = _executableNetwork.GetInputsInfo().begin()->first;
        
#ifdef HDDLPLUGIN_PROFILE  
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        HVA_DEBUG("[debug] inputsInfo duration is %ld, mode is %s\n", duration, _graphPath.c_str());

        start = std::chrono::steady_clock::now();
#endif


        IE::TensorDesc inputTensorDesc = IE::TensorDesc(IE::Precision::U8, {1, 3, heightInput, widthInput}, IE::Layout::NCHW);
        // IE::TensorDesc inputTensorDescTemp = _executableNetwork.GetInputsInfo().begin()->second->getTensorDesc();
        // assert(inputTensorDesc == inputTensorDescTemp);
        auto ptrRemoteBlob = _ptrContextIE->CreateBlob(inputTensorDesc, blobParamMap);

        assert(nullptr != ptrRemoteBlob);

#ifdef HDDLPLUGIN_PROFILE  
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        HVA_DEBUG("[debug] CreateBlob duration is %ld, mode is %s\n", duration, _graphPath.c_str());
#endif


#ifdef HDDLPLUGIN_PROFILE  
        start = std::chrono::steady_clock::now();
#endif

        // Since it 228x228 image on 224x224 network, resize preprocessing also required
        IE::PreProcessInfo preprocInfo = ptrInferRequest->GetPreProcess(_inputName);
        preprocInfo.setResizeAlgorithm(IE::RESIZE_BILINEAR);
        preprocInfo.setColorFormat(IE::ColorFormat::NV12);

        // ---- Set remote NV12 blob with preprocessing information
        ptrInferRequest->SetBlob(_inputName, ptrRemoteBlob, preprocInfo);

#ifdef INFER_FP16

        auto outputBlobName = _executableNetwork.GetOutputsInfo().begin()->first;
        auto outputBlob = ptrInferRequest->GetBlob(outputBlobName);
        auto& desc = outputBlob->getTensorDesc();

        desc.setPrecision(IE::Precision::FP32);  
#endif

#ifdef HDDLPLUGIN_PROFILE  
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        HVA_DEBUG("[debug] SetBlob duration is %ld, mode is %s\n", duration, _graphPath.c_str());
#endif

        HVA_DEBUG("[debug] end create input tensor\n");

        // std::string outputPath = "./output.bin";

        // ---- Run the request synchronously
        HVA_DEBUG("[debug] start inference\n");

#ifdef HDDLPLUGIN_PROFILE        
        start = std::chrono::steady_clock::now();
#endif
        ptrInferRequest->Wait(1000000);
        ptrInferRequest->SetCompletionCallback(callback);
        ptrInferRequest->StartAsync();
#ifdef HDDLPLUGIN_PROFILE  
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        HVA_DEBUG("[debug] hddl2plugin infer duration is %ld, mode is %s\n", duration, _graphPath.c_str());
#endif
        HVA_DEBUG("[debug] end inference\n");

        return;
    }

    inline void inferAsyncImgPipe(IE::InferRequest::Ptr ptrInferRequest, InferCallback_t callback,
                char* hostFd, size_t heightInput = 0, size_t widthInput = 0, const ROI &roi = ROI{})
    {
        //todo, fix me
        if (0 == heightInput)
        {
            heightInput = _heightInput;
        }
        else
        {
            _heightInput = heightInput;
        }
        if (0 == widthInput)
        {
            widthInput = _widthInput;
        }
        else
        {
            _widthInput = widthInput;
        }

#ifdef HDDLPLUGIN_PROFILE
        auto start = std::chrono::steady_clock::now();
#endif

#ifndef INFER_ROI
        IE::ParamMap blobParamMap = {{IE::HDDL2_PARAM_KEY(REMOTE_MEMORY_FD), static_cast<uint64_t>(remoteMemoryFd)},
                                     {IE::HDDL2_PARAM_KEY(COLOR_FORMAT), IE::ColorFormat::NV12}};
#else
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
#endif

        //todo fix me
        if (_executableNetwork.GetInputsInfo().empty())
        {
            return;
        }

        auto inputsInfo = _executableNetwork.GetInputsInfo();
        _inputName = _executableNetwork.GetInputsInfo().begin()->first;

#ifdef HDDLPLUGIN_PROFILE
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        printf("[debug] inputsInfo duration is %ld, mode is %s\n", duration, _graphPath.c_str());

        start = std::chrono::steady_clock::now();
#endif


#ifdef HDDLPLUGIN_PROFILE
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        printf("[debug] CreateBlob duration is %ld, mode is %s\n", duration, _graphPath.c_str());
#endif


#ifdef HDDLPLUGIN_PROFILE
		start = std::chrono::steady_clock::now();
#endif

		// Since it 228x228 image on 224x224 network, resize preprocessing also required
		IE::PreProcessInfo preprocInfo = ptrInferRequest->GetPreProcess(
				_inputName);
		preprocInfo.setResizeAlgorithm(IE::RESIZE_BILINEAR);
		preprocInfo.setColorFormat(IE::ColorFormat::NV12);

// --------------------------- Create a blob to hold the NV12 input data -------------------------------
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

#ifdef INFER_FP16

        auto outputBlobName = _executableNetwork.GetOutputsInfo().begin()->first;
        auto outputBlob = ptrInferRequest->GetBlob(outputBlobName);
        auto& desc = outputBlob->getTensorDesc();

        desc.setPrecision(IE::Precision::FP32);
#endif

#ifdef HDDLPLUGIN_PROFILE
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        printf("[debug] SetBlob duration is %ld, mode is %s\n", duration, _graphPath.c_str());
#endif

//        printf("[debug] end create input tensor\n");

        // std::string outputPath = "./output.bin";

        // ---- Run the request synchronously
//        printf("[debug] start inference\n");

#ifdef HDDLPLUGIN_PROFILE
        start = std::chrono::steady_clock::now();
#endif

        ptrInferRequest->SetCompletionCallback(callback);

        //Async model
        ptrInferRequest->StartAsync();

        //sync model,callback() be used in inferNode
//        ptrInferRequest->Infer();

#ifdef HDDLPLUGIN_PROFILE
        end = std::chrono::steady_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        printf("[debug] hddl2plugin infer duration is %ld, mode is %s\n", duration, _graphPath.c_str());
#endif
//        printf("[debug] end inference\n");

        return;
    }


    inline IE::Blob::Ptr getOutputBlob(IE::InferRequest::Ptr ptrInferRequest)
    {
        // --- Get output
        HVA_DEBUG("[debug] start dump output file\n");

#ifdef HDDLPLUGIN_PROFILE        
        auto start = std::chrono::steady_clock::now();
#endif
        //todo fix me
        if (_executableNetwork.GetOutputsInfo().empty())
        {
            return nullptr;
        }

        auto outputBlobName = _executableNetwork.GetOutputsInfo().begin()->first;
        auto ptrOutputBlob = ptrInferRequest->GetBlob(outputBlobName);
#ifdef HDDLPLUGIN_PROFILE  
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        HVA_DEBUG("[debug] GetBlob duration is %ld, mode is %s\n", duration, _graphPath.c_str());
#endif

        HVA_DEBUG("[debug] end dump output file\n");

        return ptrOutputBlob;
    }
    
    inline IE::InferRequest::Ptr getInferRequest()
    {
        return _ptrInferRequestPool->get();
    }
    inline void putInferRequest(IE::InferRequest::Ptr ptrInferRequest)
    {
        // ptrInferRequest->Wait(10000);
        // ptrInferRequest->SetCompletionCallback([](){}); //why does not this work
        _ptrInferRequestPool->put(ptrInferRequest);
        return;
    }

    inline void postproc(IE::Blob::Ptr ptrBlob, std::vector<ROI>& vecROI)
    {
        if (nullptr != _ptrPostproc)
        {
            _ptrPostproc(this, ptrBlob, vecROI);
        }
        return;
    }

    //todo, fix me
    inline void postprocYolotinyv2_fp16(IE::Blob::Ptr ptrBlob, std::vector<ROI>& vecROI)
    {
        //todo, fix me
#if 1
        
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

                // std::cout << "confidence = " << rawData[i * 7 + 2] << std::endl;
                // std::cout << "x0,y0,x1,y1 = " << rawData[i * 7 + 3] << ", "
                //           << rawData[i * 7 + 4] << ", "
                //           << rawData[i * 7 + 5] << ", "
                //           << rawData[i * 7 + 6] << std::endl;
            }
        }
#endif
        for (auto &object : vecObjects)
        {
            HVA_DEBUG("[debug] object detected: x is %d, y is %d, w is %d, h is %d\n", object.x, object.y, object.width, object.height);
            cv::rectangle(_frameBGR, cv::Rect(object.x, object.y, object.width, object.height), cv::Scalar(0, 255, 0), 2);
        }
        // char filename[256];
        // static int frameCnt = 0;
        // snprintf(filename, 256, "./output/debug-output-%d.jpg", frameCnt);
        // frameCnt++;
        // std::cout << filename << std::endl;
        // cv::imwrite(filename, frameBGR);
        // cv::imshow("output", frameBGR);
        // cv::waitKey(10);

        return;
    }

    inline void postprocYolotinyv2_u8(IE::Blob::Ptr ptrBlob, std::vector<ROI>& vecROI)
    {
        //todo, fix me
#if 0
        std::vector<DetectedObject_t> &vecObjects;
        vecObjects = ::YoloV2Tiny::TensorToBBoxYoloV2TinyCommon(_ptrOutputBlob, _heightInput, _widthInput,
                                                               .6, YoloV2Tiny::fillRawNetOut);
#else

#ifdef HDDLPLUGIN_PROFILE
        auto start = std::chrono::steady_clock::now();
#endif
        auto blobDequant = deQuantize(ptrBlob, 0.271045, 210);
#ifdef HDDLPLUGIN_PROFILE
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        HVA_DEBUG("[debug] dequant duration is %ld, mode is %s\n", duration, _graphPath.c_str());
#endif
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
        vecROI.clear();

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
                ROI object;
                object.x = (rawData[i * 7 + 3]) * _widthInput / 416;
                object.y = (rawData[i * 7 + 4]) * _heightInput / 416;
                object.width = (rawData[i * 7 + 5] - rawData[i * 7 + 3]) * _widthInput / 416;
                object.height = (rawData[i * 7 + 6] - rawData[i * 7 + 4]) * _heightInput / 416;
                object.confidenceDetection = rawData[i * 7 + 2];
                vecROI.push_back(object);

                // std::cout << "confidence = " << rawData[i * 7 + 2] << std::endl;
                // std::cout << "x0,y0,x1,y1 = " << rawData[i * 7 + 3] << ", "
                //           << rawData[i * 7 + 4] << ", "
                //           << rawData[i * 7 + 5] << ", "
                //           << rawData[i * 7 + 6] << std::endl;
            }
        }
#endif
        for (auto &object : vecROI)
        {
            HVA_DEBUG("[debug] object detected: x is %d, y is %d, w is %d, h is %d\n", object.x, object.y, object.width, object.height);
            cv::rectangle(_frameBGR, cv::Rect(object.x, object.y, object.width, object.height), cv::Scalar(0, 255, 0), 2);
        }
#if 0 //only for debug
        char filename[256];
        static int frameCnt = 0;
        snprintf(filename, 256, "./output/debug-output-%d.jpg", frameCnt);
        frameCnt++;
        std::cout << filename << std::endl;
        cv::imwrite(filename, _frameBGR);
        // cv::imshow("output", frameBGR);
        // cv::waitKey(10);
#endif
        return;
    }

    //todo, fix me
    inline void postprocResnet50_fp16(IE::Blob::Ptr ptrBlob, std::vector<ROI>& vecROI)
    {
        //todo, fix me
        float* ptrFP32 = ptrBlob->buffer().as<float*>();

        size_t outputSize = 1000;

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
        // std::vector<std::string> labels = readLabelsFromFile("/home/kmb/cong/graph/resnet.labels");
        roi.labelClassification = m_labels.imagenet_labelstring(idx);
        roi.confidenceClassification = exp(max) / sum;
        HVA_DEBUG("[debug] roi label is : %s\n", m_labels.imagenet_labelstring(idx).c_str());

        //todo fix me
        assert(vecROI.size() == 0);

        vecROI.push_back(roi);

        return;
    }

#ifndef SINGLE_ROI_POSTPROC
    //todo, fix me
    inline void postprocResnet50_u8(IE::Blob::Ptr ptrBlob, std::vector<ROI>& vecROI)
    {
        //todo, fix me
        // std::vector<int>& vecIdx = _vecIdx;
        // std::vector<std::string>& vecLabel = _vecLabel;
        // std::vector<float>& vecConfidence = _vecConfidence;
        // vecIdx.clear();
        // vecLabel.clear();
        // vecConfidence.clear();

        auto blobDequant = deQuantize(ptrBlob, 0.151837, 67);
        size_t outputSize = 1000;
        
        //todo, fix me single roi
        if (vecROI.size() > 0)
        {
            float *ptrFP32_ROI = blobDequant->buffer().as<float*>();
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
            for (int i = 0; i < vecROI.size(); i++)
            {
                vecROI[i].labelIdClassification = idx;
                // std::vector<std::string> labels = readLabelsFromFile("/home/kmb/cong/graph/resnet.labels");
                // _vecROI[i].label = labels[idx];
                vecROI[i].labelClassification = m_labels.imagenet_labelstring(idx);
                vecROI[i].confidenceClassification = exp(max) / sum;
                HVA_DEBUG("[debug] roi label is : %s\n", m_labels.imagenet_labelstring(idx).c_str());
            }
        }

        return;
    }
#else //#ifndef SINGLE_ROI_POSTPROC
    //todo, fix me
    inline void postprocResnet50_u8(IE::Blob::Ptr ptrBlob, std::vector<ROI>& vecROI)
    {
        //todo, fix me
        // std::vector<int>& vecIdx = _vecIdx;
        // std::vector<std::string>& vecLabel = _vecLabel;
        // std::vector<float>& vecConfidence = _vecConfidence;
        // vecIdx.clear();
        // vecLabel.clear();
        // vecConfidence.clear();

        auto blobDequant = deQuantize(ptrBlob, 0.151837, 67);
        size_t outputSize = 1000;
        
        float *ptrFP32_ROI = blobDequant->buffer().as<float*>();
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
        // std::vector<std::string> labels = readLabelsFromFile("/home/kmb/cong/graph/resnet.labels");
        // _vecROI[i].label = labels[idx];
        roi.labelClassification = m_labels.imagenet_labelstring(idx);
        roi.confidenceClassification = exp(max) / sum;
        HVA_DEBUG("[debug] roi label is : %s\n", m_labels.imagenet_labelstring(idx).c_str());
        
        vecROI.push_back(roi);

        return;
    }
#endif //#ifndef SINGLE_ROI_POSTPROC
    template <int N>
    inline static int alignTo(int x)
    {
        return ((x + N - 1) & ~(N - 1));
    }

    inline static int compile(std::string& graphName)
    {
        HVA_DEBUG("compile start : %s\n", graphName.c_str());
        InferenceEngine::Core ie;
        auto network = ie.ReadNetwork(graphName, graphName.substr(0, graphName.length() - 4) + ".bin");
        
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
        auto executableNetwork = ie.LoadNetwork(network, "HDDL2", {});

        auto outputName = graphName.substr(0, graphName.length() - 4) + ".blob";
        executableNetwork.Export(outputName);
        HVA_DEBUG("compile end : %s\n", outputName.c_str());
    }

public:
    static IE::Blob::Ptr deQuantize(const IE::Blob::Ptr &quantBlob, float scale, uint8_t zeroPoint)
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

    static IE::Blob::Ptr yoloLayer_yolov2tiny(const IE::Blob::Ptr &lastBlob, int inputHeight, int inputWidth)
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

    static std::vector<std::string> readLabelsFromFile(const std::string &labelFileName)
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

public:
    class InferRequestPool_t
    {
    public:
        InferRequestPool_t() = delete;
        explicit InferRequestPool_t(IE::ExecutableNetwork& executableNetwork, int32_t numInferRequest)
        : _cntInferRequest{numInferRequest}, _maxSize{static_cast<size_t>(numInferRequest)}
        {
            for (int32_t i = 0; i < numInferRequest; i++)
            {
                auto ptrInferRequest = executableNetwork.CreateInferRequestPtr();
                _vecInferRequest.push_back(ptrInferRequest);
                // _mapRequest2Status[ptrInferRequest] = InferRequestStatus_t::UNUSED;
                _queue.push(ptrInferRequest);
            }
        }
        ~InferRequestPool_t()
        {
            for(auto ptrInferRequest : _vecInferRequest)
            {
                ptrInferRequest->Wait(1000000);
            }
        }
        InferRequestPool_t(const InferRequestPool_t&) = delete;
        InferRequestPool_t& operator= (const InferRequestPool_t&) = delete;

        enum class InferRequestStatus_t
        {
            UNUSED,
            USED
        };


        // //todo fix me
        // inline void insert(const IE::InferRequest::Ptr ptrInferRequest)
        // {
        //     {
        //         std::lock_guard<std::mutex> lock{_mutex};
        //         _cntInferRequest++;
        //         _vecInferRequest.push_back(ptrInferRequest);
        //         // _mapRequest2Status[ptrInferRequest] = InferRequestStatus_t::UNUSED;
        //         _freeInferRequest.push(ptrInferRequest);
        //     }
        // }

        // //todo fix me
        // inline void clear()
        // {
        //     {
        //         std::lock_guard<std::mutex> lock{_mutex};
        //         _cntInferRequest = 0;
        //         _vecInferRequest.clear();
        //         // _mapRequest2Status.clear();
        //     }

        // }

    private:
        using Type = IE::InferRequest::Ptr;
    
    public:
        inline Type get()
        {
            Type ptr = nullptr;
            pop(ptr);
            return ptr;
        }

        inline void put(const Type ptrInferRequest)
        {
            push(ptrInferRequest);
        }

    private:
        // using Type = IE::InferRequest::Ptr;
        bool push(Type value)
        {
            std::unique_lock<std::mutex> lk(_mutex);             
            _cv_full.wait(lk,[this]{return (_queue.size() <= _maxSize || true == _close);});
            if (true == _close)
            {
                return false;
            }   
            _queue.push(std::move(value));
            _cv_empty.notify_one();
            return true;
        }

        // bool tryPush(Type value)
        // {
        //     std::lock_guard<std::mutex> lk(_mutex);
        //     if (true == _close)
        //     {
        //         return false;
        //     }
        //     if(_queue.size() >= _maxSize)
        //     {
        //         return false;
        //     }
        //     _queue.push(value);
        //     _cv_empty.notify_one();
        //     return true;
        // }

        bool pop(Type& value)
        {
            std::unique_lock<std::mutex> lk(_mutex);
            _cv_empty.wait(lk,[this]{return (!_queue.empty() || true == _close);});
            if (true == _close)
            {
                return false;
            }
            value=_queue.front();
            _queue.pop();
            _cv_full.notify_one();
            return true;
        } 

        // bool tryPop(Type& value)
        // {
        //     std::lock_guard<std::mutex> lk(_mutex);
        //     if (true == _close)
        //     {
        //         return false;
        //     }
        //     if(_queue.empty())
        //     {
        //         return false;
        //     }
        //     value=std::move(_queue.front());
        //     _queue.pop();
        //     _cv_full.notify_one();
        //     return true;
        // }

        bool empty() const
        {
            std::lock_guard<std::mutex> lk(_mutex);
            return _queue.empty();
        }

        void close()
        {
            std::lock_guard<std::mutex> lk(_mutex);
            _close = true;
            _cv_empty.notify_all();
            _cv_full.notify_all();
            return;
        }

    private:
        mutable std::mutex _mutex;
        std::queue<Type> _queue;
        std::condition_variable _cv_empty;
        std::condition_variable _cv_full;
        std::size_t _maxSize; //max queue size
        bool _close{false};

    private:
        std::vector<Type> _vecInferRequest;
        // std::unordered_map<IE::InferRequest::Ptr, InferRequestStatus_t> _mapRequest2Status;

        int32_t _cntInferRequest{0};

    public:
        using Ptr = std::shared_ptr<InferRequestPool_t>;

    };

    class OrderKeeper_t
    {
    public:
        OrderKeeper_t()
        {
        }

        OrderKeeper_t(const OrderKeeper_t&) = delete;
        OrderKeeper_t& operator=(const OrderKeeper_t&) = delete;

        inline void lock(uint64_t id)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            HVA_DEBUG("[debug] order keeper lock\n");
            if(id < _cntOrder)
            {
                HVA_DEBUG("[warning] duplicated order id\n");
            }
            _cv.wait(lock, [&]{return id <= _cntOrder;});
        }

        inline void unlock(uint64_t id)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            HVA_DEBUG("[debug] order keeper unlock\n");
            //todo, fix me
            assert(id == _cntOrder);
            if(id < _cntOrder)
            {
                HVA_DEBUG("[warning] duplicated order id\n");
            }
            else
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

        inline void bypass(uint64_t id)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            HVA_DEBUG("[debug] order keeper bypass\n");
            
            if(id <= _cntOrder)
            {
                if(id < _cntOrder)
                {
                    HVA_DEBUG("[warning] duplicated order id\n");
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

    private:
        uint64_t _cntOrder{0};
        std::mutex _mutex;
        std::condition_variable _cv;
        std::set<uint64_t> _setFrameId;
    public:
        using Ptr = std::shared_ptr<OrderKeeper_t>;
    };

private:
    IE::Core _ie;
    IE::RemoteContext::Ptr _ptrContextIE;
    IE::ExecutableNetwork _executableNetwork;
    IE::InferRequest::Ptr _ptrInferRequest;
    InferRequestPool_t::Ptr _ptrInferRequestPool;


    IE::RemoteBlob::Ptr _ptrRemoteBlob;
    IE::Blob::Ptr _ptrOutputBlob;
    std::string _inputName;

    HddlUnite::WorkloadContext::Ptr _ptrContext;
    WorkloadID _workloadId{0ul};
    int _remoteMemoryFd{0};

    std::string _graphPath;
    size_t _heightInput{0};
    size_t _widthInput{0};

    //todo, config
    float _thresholdDetection{0.6f};

    PostprocPtr_t _ptrPostproc{nullptr};



public:
    // std::vector<DetectedObject_t> _vecObjects;
    // std::vector<ROI> _vecROI;

    static ImageNetLabels m_labels;
    //todo, only for test
    cv::Mat _frameBGR;
    HddlUnite::SMM::RemoteMemory::Ptr _remoteFrame;
    std::string _inputPath;
};

#endif //#ifndef HDDL2PLUGIN_HELPER_HPP
