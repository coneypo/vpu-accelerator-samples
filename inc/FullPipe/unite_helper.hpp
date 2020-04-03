#pragma once
#include "RemoteMemory.h"
#include "InferGraph.h"
#include <InferBlob.h>
#include "WorkloadContext.h"
#include <HddlUnite.h>
#include <Inference.h>

#include <ie_compound_blob.h>
#include <ie_iextension.h>

#include "stdio.h"
#include <vector>
#include <memory>
#include <string>
#include <iostream>

#include <opencv2/opencv.hpp>

#include "detection_helper.hpp"
#include "tinyYolov2_post.h"

//------------------------------------------------------------------------------
//      class WorkloadContext_Helper
//------------------------------------------------------------------------------
class WorkloadContext_Helper {
public:
    WorkloadContext_Helper(WorkloadID id = -1);
    ~WorkloadContext_Helper();

    static WorkloadID createAndRegisterWorkloadContext();
    static void destroyHddlUniteContext(const WorkloadID& id);
    static bool isValidWorkloadContext(const WorkloadID& id);

    WorkloadID getWorkloadId() const;
    HddlUnite::WorkloadContext::Ptr getWorkloadContext() const;
protected:
    WorkloadID _workloadId;
};


//------------------------------------------------------------------------------
//      class RemoteMemory_Helper
//------------------------------------------------------------------------------
using RemoteMemoryFd = uint64_t ;

class RemoteMemory_Helper {
public:
    RemoteMemoryFd allocateRemoteMemory(const WorkloadID &id, const size_t& size);
    RemoteMemoryFd allocateRemoteMemory(const WorkloadID &id,
            const InferenceEngine::TensorDesc& tensorDesc);
    void destroyRemoteMemory();

    std::string getRemoteMemory(const size_t &size);
    bool isRemoteTheSame(const std::string &dataToCompare);
    void setRemoteMemory(const std::string& dataToSet);

    virtual ~RemoteMemory_Helper();

private:
    HddlUnite::SMM::RemoteMemory::Ptr _memory = nullptr;
    RemoteMemoryFd _memoryFd = 0;

};

//------------------------------------------------------------------------------
//      class HddlUnite_Graph_Helper
//------------------------------------------------------------------------------
class HddlUnite_Graph_Helper {
public:
    using Ptr = std::shared_ptr<HddlUnite_Graph_Helper>;

    HddlUnite_Graph_Helper();
    HddlUnite_Graph_Helper(const std::string& graphName, const std::string& graphPath, 
                            const HddlUnite::WorkloadContext& workloadContext);
    HddlUnite::Inference::Graph::Ptr getGraph();

protected:
    HddlUnite::Inference::Graph::Ptr _graphPtr = nullptr;

    std::string _graphName = "resnet";
    std::string _graphPath = "/home/kmb/cong/graph/resnet-50-dpu/resnet-50-dpu.blob";
};


namespace hva
{
    
// Function to convert F32 into F16
// F32: exp_bias:127 SEEEEEEE EMMMMMMM MMMMMMMM MMMMMMMM.
// F16: exp_bias:15  SEEEEEMM MMMMMMMM
#define HVA_EXP_MASK_F32 0x7F800000U
#define HVA_EXP_MASK_F16 0x7C00U

// Function to convert F32 into F16
float f16tof32(short x);
void f16tof32Arrays(float* dst, const short* src, size_t nelem, float scale = 1.0f, float bias = 0.0f);
}


namespace hva
{



class UniteHelper
{
public:

    UniteHelper(std::string graphName, std::string graphPath, int32_t videoWidth, int32_t videoHeight, bool needPP,
                 int32_t inputSizeNN, int32_t outputSize, WorkloadID id = -1,  RemoteMemoryFd _remoteMemoryFd = 0ul);
    UniteHelper(WorkloadID id, std::string graphName, std::string graphPath, int32_t inputSizeNN, int32_t outputSize,
                RemoteMemoryFd _remoteMemoryFd = 0ul);
    UniteHelper() = default;

    void callInferenceOnBlobs(RemoteMemoryFd remoteMemoryFd = 0ul, const std::vector<InfoROI_t>& vecROI = std::vector<InfoROI_t>{});
    void setup();
    void update(int32_t videoWidth, int32_t videoHeight, uint64_t fd = 0ul, 
                const std::vector<InfoROI_t>& vecROI = std::vector<InfoROI_t>{});

    std::vector<DetectedObject_t> _vecOjects;
    std::vector<int> _vecIdx;
    std::vector<std::string> _vecLabel;
    std::vector<float> _vecConfidence;
    std::vector<InfoROI_t> _vecROI;

#if 1
    std::string graphName = "yolotiny";
    // std::string graphPath = "/home/kmb/cong/graph/opt/yolotiny/yolotiny.blob";
    std::string graphPath = "/home/kmb/cong/fp16_test/yolotiny/tiny_yolo_v2_uint8_int8_weights_pertensor.blob";
    std::string inputName = "input_name";
    std::string outputName = "output_name";
    // int32_t inputSizePP = 1080*1080*3/2;
    int32_t inputSizePP = 1080*1920*3/2;
    // int32_t inputSizePP = 519168;
    int32_t inputSizeNN = 519168;
    // int32_t outputSize = 21125;
    int32_t outputSize = 42250;

    // in/out info
    // std::string inputPath = "/home/kmb/cong/graph/resnet-50-dpu/input-cat-1080x1080-nv12.bin";
    // std::string inputPath = "/home/kmb/cong/fp16_test/yolotiny/input.dat";
    std::string inputPath = "/home/kmb/cong/nv12/car/9.nv12";
    // std::string outputPath = "/home/kmb/cong/graph/yolotiny/output.bin";
    HddlUnite::Inference::Precision precision = HddlUnite::Inference::Precision::U8;
    int32_t videoWidth = 1920;
    int32_t videoHeight = 1080;
    int32_t videoWidthStride = 1920;

    bool needPP = true;
# else
    std::string graphName = "resnet";
    // std::string graphPath = "/home/kmb/cong/graph/resnet-50-dpu/resnet-50-dpu.blob";
    // std::string graphPath = "/home/kmb/cong/graph/opt/resnet/resnet.blob";
    std::string graphPath = "/home/kmb/cong/graph/alpha/resnet50_uint8_int8_weights_pertensor.blob";
    std::string inputName = "input_name";
    std::string outputName = "output_name";
    int32_t inputSizePP = 1080*1080*3/2;
    // int32_t inputSizePP = 150528;
    int32_t inputSizeNN = 150528;
    int32_t outputSize = 2000;

    // in/out info
    std::string inputPath = "/home/kmb/cong/graph/resnet-50-dpu/input-cat-1080x1080-nv12.bin";
    // std::string inputPath = "/home/kmb/cong/graph/resnet-50-dpu/input.bin";
    // std::string outputPath = "/home/kmb/cong/graph/resnet-50-dpu/output.bin";
    HddlUnite::Inference::Precision precision = HddlUnite::Inference::Precision::U8;
    int32_t videoWidth = 1080;
    int32_t videoHeight = 1080;
    int32_t videoWidthStride = 1080;

    bool needPP = true;
#endif

protected:
    // graph info
    WorkloadID _workloadId = -1;
    HddlUnite::SMM::RemoteMemory::Ptr _remoteMemoryPtr = nullptr;



    HddlUnite::Inference::InferData::Ptr _inferDataPtr = nullptr;
    RemoteMemoryFd _remoteMemoryFd = 0;
    WorkloadContext_Helper _workloadContextHelper{static_cast<WorkloadID>(-1ul)};
    RemoteMemory_Helper _remoteMemoryHelper;

protected:
    std::vector<HddlUnite::Inference::AuxBlob::Type> _auxBlob {HddlUnite::Inference::AuxBlob::Type::TimeTaken};
    HddlUnite_Graph_Helper::Ptr _uniteGraphHelper = nullptr;
};

}