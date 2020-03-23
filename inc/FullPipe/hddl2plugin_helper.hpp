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

#include <fstream>

// using namespace InferenceEngine;
namespace IE = InferenceEngine;

class HDDL2pluginHelper_t
{
public:
    HDDL2pluginHelper_t() = default;
    ~HDDL2pluginHelper_t() = default;
    HDDL2pluginHelper_t(const HDDL2pluginHelper_t&) = delete;
    HDDL2pluginHelper_t& operator=(const HDDL2pluginHelper_t&) = delete;

    inline void setup()
    {
        std::string graphPath = "/home/kmb/cong/graph/resnet-50-dpu/resnet-50-dpu.blob";
        std::string inputPath = "/home/kmb/cong/graph/resnet-50-dpu/input.bin";
        std::string outputPath = "./output.bin";
        
        inputPath = "/home/kmb/cong/graph/resnet-50-dpu/input-cat-1080x1080-nv12.bin";

        // graphPath = "/home/kmb/cong/fp16_test/resnet/resnet50_uint8_int8_weights_pertensor.blob";
        // inputPath = "/home/kmb/cong/fp16_test/resnet/input.bin";
        // outputPath = "./output.bin";


        printf("[debug] start create context\n");
        ptrContext = HddlUnite::createWorkloadContext();
        assert(nullptr != ptrContext.get());

        ptrContext->setContext(workloadId);
        assert(workloadId == ptrContext->getWorkloadContextID());
        assert(HddlStatusCode::HDDL_OK == HddlUnite::registerWorkloadContext(ptrContext));
        printf("[debug] end create context\n");

        // ---- Load frame to remote memory (emulate VAAPI result)
        // ----- Load binary input
        printf("[debug] start create input tensor\n");

        //todo, no need to load data from file
        const auto& inputTensor = InferenceEngine::TensorDesc(InferenceEngine::Precision::U8,
                                                               {1, 1, 1, 1080*1080*3/2},
                                                               InferenceEngine::Layout::NCHW);
        auto inputRefBlob = IE::make_shared_blob<uint8_t>(inputTensor);
        inputRefBlob->allocate();
        
        
        // (vpu::KmbPlugin::utils::fromBinaryFile(inputPath, inputRefBlob));
        FILE* fp;
        
        printf("inputRefBlob->byteSize() is :%d\n", inputRefBlob->byteSize());
        fp = fopen(inputPath.c_str(), "rb");
        fread(inputRefBlob->buffer().as<void*>(), inputRefBlob->byteSize(), 1, fp);
        fclose(fp);


        fp = fopen("originalInput.bin", "wb");
        fwrite(inputRefBlob->buffer().as<void*>(), inputRefBlob->byteSize(), 1, fp);
        fclose(fp);

        // ----- Allocate memory with HddlUnite on device
#if 0
        remoteMemoryFd =
            allocateRemoteMemory(ptrContext, inputRefBlob->buffer().as<void*>(), inputRefBlob->byteSize());
#else
        HddlUnite::SMM::RemoteMemory::Ptr remoteFrame = HddlUnite::SMM::allocate(*ptrContext, inputRefBlob->byteSize());

        if (remoteFrame == nullptr) {
            THROW_IE_EXCEPTION << "Failed to allocate remote memory.";
        }

        if (remoteFrame->syncToDevice(inputRefBlob->buffer().as<void*>(), inputRefBlob->byteSize()) != HDDL_OK) {
            THROW_IE_EXCEPTION << "Failed to sync memory to device.";
        }
        remoteMemoryFd = remoteFrame->getDmaBufFd();
#endif

        printf("[debug] end create input tensor\n");

        HddlUnite::SMM::RemoteMemory memTemp(*ptrContext, remoteMemoryFd, inputRefBlob->byteSize());

        char* buf = (char*)malloc(inputRefBlob->byteSize());
        memTemp.syncFromDevice(buf, inputRefBlob->byteSize());
        
        // std::ofstream fileRead("./readInput.bin", std::ios::out | std::ios::binary);
        // fileRead.write(buf, inputRefBlob->byteSize());
        fp = fopen("remoteInput.bin", "wb");
        fwrite(buf, 1 , inputRefBlob->byteSize(), fp);
        fclose(fp);

        free(buf);

        // ---- Load inference engine instance
        printf("[debug] start load graph\n");
        

        // ---- Init context map and create context based on it
        IE::ParamMap paramMap = {{IE::HDDL2_PARAM_KEY(WORKLOAD_CONTEXT_ID), workloadId}};
        ptrContextIE = ie.CreateContext("HDDL2", paramMap);

        // ---- Import network providing context as input to bind to context
        const std::string& modelPath = graphPath;

        std::filebuf blobFile;
        if (!blobFile.open(modelPath, std::ios::in | std::ios::binary)) {
            blobFile.close();
            THROW_IE_EXCEPTION << "Could not open file: " << modelPath;
        }
        std::istream graphBlob(&blobFile);

        executableNetwork = ie.ImportNetwork(graphBlob, ptrContextIE);
        printf("[debug] end load graph\n");

        // ---- Create infer request
        printf("[debug] start inference\n");
        inferRequest = executableNetwork.CreateInferRequest();

        // ---- Create remote blob by using already exists fd
#if 0
        IE::ParamMap blobParamMap = {{IE::HDDL2_PARAM_KEY(REMOTE_MEMORY_FD), remoteMemoryFd}};
#else
        IE::ParamMap blobParamMap = {{IE::HDDL2_PARAM_KEY(REMOTE_MEMORY_FD), remoteMemoryFd},
            {IE::HDDL2_PARAM_KEY(COLOR_FORMAT), IE::ColorFormat::NV12}};
#endif
        auto inputsInfo = executableNetwork.GetInputsInfo();
        inputName = executableNetwork.GetInputsInfo().begin()->first;

#if 0
        IE::InputInfo::CPtr inputInfoPtr = executableNetwork.GetInputsInfo().begin()->second;
        ptrRemoteBlob = ptrContextIE->CreateBlob(inputInfoPtr->getTensorDesc(), blobParamMap);
#else
        IE::TensorDesc inputTensorDesc = IE::TensorDesc(IE::Precision::U8, {1, 3, 1080, 1080}, IE::Layout::NCHW);
        // IE::TensorDesc inputTensorDesc = executableNetwork.GetInputsInfo().begin()->second->getTensorDesc();
        ptrRemoteBlob = ptrContextIE->CreateBlob(inputTensorDesc, blobParamMap);
#endif
        assert(nullptr != ptrRemoteBlob);

        printf("[debug] start dump input\n");
        std::ofstream fileInput("./input.bin", std::ios_base::out | std::ios_base::binary);

        const size_t inputSize = ptrRemoteBlob->byteSize();
        fileInput.write(ptrRemoteBlob->buffer(), inputSize);
        printf("[debug] end dump input\n");


#if 0
        // ---- Set remote blob as input for infer request         
        inferRequest.SetBlob(inputName, ptrRemoteBlob);
#else
        // Since it 228x228 image on 224x224 network, resize preprocessing also required
        IE::PreProcessInfo preprocInfo = inferRequest.GetPreProcess(inputName);
        preprocInfo.setResizeAlgorithm(IE::RESIZE_BILINEAR);
        preprocInfo.setColorFormat(IE::ColorFormat::NV12);

        // ---- Set remote NV12 blob with preprocessing information
        inferRequest.SetBlob(inputName, ptrRemoteBlob, preprocInfo);
#endif
        // ---- Run the request synchronously
        inferRequest.Infer();
        printf("[debug] end inference\n");

        // --- Get output
        printf("[debug] start dump output file\n");
        auto outputBlobName = executableNetwork.GetOutputsInfo().begin()->first;
        ptrOutputBlob = inferRequest.GetBlob(outputBlobName);

        std::ofstream file(outputPath, std::ios_base::out | std::ios_base::binary);
        if (!file.good() || !file.is_open()) {
            std::stringstream err;
            err << "Cannot access output file. File path: " << outputPath;
            throw std::runtime_error(err.str());
        }

        const size_t outputSize = ptrOutputBlob->byteSize();
        file.write(ptrOutputBlob->buffer(), outputSize);
        printf("[debug] end dump output file\n");

        assert(ptrOutputBlob->getTensorDesc().getPrecision() == IE::Precision::U8);
    }
    inline void update()
    {

    }
    inline void infer()
    {

    }

public:
    IE::Core ie;
    IE::RemoteContext::Ptr ptrContextIE;
    IE::ExecutableNetwork executableNetwork;
    IE::InferRequest inferRequest;
    IE::RemoteBlob::Ptr ptrRemoteBlob;
    IE::Blob::Ptr ptrOutputBlob;
    std::string inputName;

    HddlUnite::WorkloadContext::Ptr ptrContext;
    WorkloadID workloadId;
    uint64_t remoteMemoryFd;
};

#endif //#ifndef HDDL2PLUGIN_HELPER_HPP