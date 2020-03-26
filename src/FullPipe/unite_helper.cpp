#include "unite_helper.hpp"

#include "RemoteMemory.h"
#include "InferGraph.h"
#include <InferBlob.h>
#include "WorkloadContext.h"
#include <HddlUnite.h>
#include <Inference.h>

// #include <ie_compound_blob.h>
// #include <ie_iextension.h>

#include "stdio.h"
#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include <fstream>

#include <opencv2/opencv.hpp>

#include "detection_helper.hpp"
#include "tinyYolov2_post.h"




//------------------------------------------------------------------------------
//      class WorkloadContext_Helper Implementation
//------------------------------------------------------------------------------
WorkloadContext_Helper::WorkloadContext_Helper(WorkloadID id) {
    if (-1ul == id)
    {
        _workloadId = createAndRegisterWorkloadContext();
    }
    else
    {
        _workloadId = id;
    }
    
}

WorkloadContext_Helper::~WorkloadContext_Helper() {
    destroyHddlUniteContext(_workloadId);
}

WorkloadID WorkloadContext_Helper::createAndRegisterWorkloadContext() {
    WorkloadID id = -1;
    auto context = HddlUnite::createWorkloadContext();

    auto ret = context->setContext(id);
    if (ret != HDDL_OK) {
        throw std::runtime_error("Error: WorkloadContext set context failed\"");
    }
    ret = registerWorkloadContext(context);
    if (ret != HDDL_OK) {
        throw std::runtime_error("Error: WorkloadContext register on WorkloadCache failed");
    }

    return id;
}

void WorkloadContext_Helper::destroyHddlUniteContext(const WorkloadID &id) {
    HddlUnite::unregisterWorkloadContext(id);
}

WorkloadID WorkloadContext_Helper::getWorkloadId() const {
    return _workloadId;
}

inline bool WorkloadContext_Helper::isValidWorkloadContext(const WorkloadID &id) {
    HddlUnite::WorkloadContext::Ptr workloadContext = HddlUnite::queryWorkloadContext(id);
    return workloadContext != nullptr;
}

HddlUnite::WorkloadContext::Ptr WorkloadContext_Helper::getWorkloadContext() const {
    return HddlUnite::queryWorkloadContext(_workloadId);
}




#include "ie_layouts.h"
template <typename TIterator>
auto product(TIterator beg, TIterator en) -> typename std::remove_reference<decltype(*beg)>::type {
    return std::accumulate(beg, en, static_cast<typename std::remove_reference<decltype(*beg)>::type>(1),
                           std::multiplies<typename std::remove_reference<decltype(*beg)>::type>());
}

//------------------------------------------------------------------------------
//      class RemoteMemory_Helper
//------------------------------------------------------------------------------
using RemoteMemoryFd = uint64_t ;
// Emulator limit 4MB
constexpr size_t EMULATOR_MAX_ALLOC_SIZE = static_cast<size_t>(0x1u << 22u);


//------------------------------------------------------------------------------
//      class RemoteMemory_Helper Implementation
//------------------------------------------------------------------------------
RemoteMemory_Helper::~RemoteMemory_Helper() {
    destroyRemoteMemory();
}

RemoteMemoryFd RemoteMemory_Helper::allocateRemoteMemory(const WorkloadID &id,
                                                                const InferenceEngine::TensorDesc& tensorDesc) {
    const size_t size = product(
            tensorDesc.getDims().begin(), tensorDesc.getDims().end());
    return allocateRemoteMemory(id, size);
}

RemoteMemoryFd
RemoteMemory_Helper::allocateRemoteMemory(const WorkloadID &id, const size_t &size) {
    if (_memory != nullptr) {
        printf("Memory already allocated!\n");
        return 0;
    }

    HddlUnite::WorkloadContext::Ptr context = HddlUnite::queryWorkloadContext(id);
    if (context == nullptr) {
        printf("Incorrect workload id!\n");
        return 0;
    }

    _memory = HddlUnite::SMM::allocate(*context, size);
    if (_memory == nullptr) {
        return 0;
    }

    _memoryFd = _memory->getDmaBufFd();

    printf("Memory fd: %lu\n", _memoryFd);
    return _memoryFd;
}

void RemoteMemory_Helper::destroyRemoteMemory() {
    _memoryFd = 0;
    _memory = nullptr;
}

std::string RemoteMemory_Helper::getRemoteMemory(const size_t &size) {
    char tempBuffer[EMULATOR_MAX_ALLOC_SIZE] = {};
    auto retCode = _memory->syncFromDevice(tempBuffer, size);
    if (retCode != HDDL_OK) {
        printf("[ERROR] Failed to sync memory from device!\n");
        return "";
    }
    return std::string(tempBuffer);
}

bool RemoteMemory_Helper::isRemoteTheSame(const std::string &dataToCompare) {
    const size_t size = dataToCompare.size();
    const std::string remoteMemory = getRemoteMemory(size);
    if (dataToCompare != remoteMemory) {
        std::cout << "Handle: " << _memoryFd << " Remote memory " << remoteMemory
                     << " != local memory " << dataToCompare << std::endl;
        return false;
    }
    return true;
}

void RemoteMemory_Helper::setRemoteMemory(const std::string& dataToSet) {
    _memory->syncToDevice(dataToSet.data(), dataToSet.size());
}




//------------------------------------------------------------------------------
//      class HddlUnite_Graph_Helper Implementation
//------------------------------------------------------------------------------
HddlUnite_Graph_Helper::HddlUnite_Graph_Helper() {
    HddlStatusCode statusCode = HddlUnite::Inference::loadGraph(
            _graphPtr, _graphName, _graphPath);
    if (statusCode != HDDL_OK) {
        THROW_IE_EXCEPTION << "Failed to load graph";
    }
}

HddlUnite_Graph_Helper::HddlUnite_Graph_Helper(const std::string& graphName, const std::string& graphPath, 
                                                        const HddlUnite::WorkloadContext& workloadContext) {
    _graphName = graphName;
    _graphPath = graphPath;

    HddlStatusCode statusCode = HddlUnite::Inference::loadGraph(
            _graphPtr, _graphName, _graphPath, {workloadContext});
    if (statusCode != HDDL_OK) {
        THROW_IE_EXCEPTION << "Failed to load graph";
    }
}

HddlUnite::Inference::Graph::Ptr HddlUnite_Graph_Helper::getGraph() {
    return _graphPtr;
}

namespace hva
{
    
// Function to convert F32 into F16
// F32: exp_bias:127 SEEEEEEE EMMMMMMM MMMMMMMM MMMMMMMM.
// F16: exp_bias:15  SEEEEEMM MMMMMMMM
// Function to convert F32 into F16
float f16tof32(short x) {
    // this is storage for output result
    uint32_t u = static_cast<uint32_t>(x);

    // get sign in 32bit format
    uint32_t s = ((u & 0x8000) << 16);

    // check for NAN and INF
    if ((u & HVA_EXP_MASK_F16) == HVA_EXP_MASK_F16) {
        // keep mantissa only
        u &= 0x03FF;

        // check if it is NAN and raise 10 bit to be align with intrin
        if (u) {
            u |= 0x0200;
        }

        u <<= (23 - 10);
        u |= HVA_EXP_MASK_F32;
        u |= s;
    } else if ((u & HVA_EXP_MASK_F16) == 0) {  // check for zero and denormals.
        uint16_t h_sig = (u & 0x03ffu);
        if (h_sig == 0) {
            /* Signed zero */
            u = s;
        } else {
            /* Subnormal */
            uint16_t h_exp = (u & HVA_EXP_MASK_F16);
            h_sig <<= 1;
            while ((h_sig & 0x0400u) == 0) {
                h_sig <<= 1;
                h_exp++;
            }
            uint32_t f_exp = (static_cast<uint32_t>(127 - 15 - h_exp)) << 23;
            uint32_t f_sig = (static_cast<uint32_t>(h_sig & 0x03ffu)) << 13;
            u = s + f_exp + f_sig;
        }
    } else {
        // abs
        u = (u & 0x7FFF);

        // shift mantissa and exp from f16 to f32 position
        u <<= (23 - 10);

        // new bias for exp (f16 bias is 15 and f32 bias is 127)
        u += ((127 - 15) << 23);

        // add sign
        u |= s;
    }

    // finaly represent result as float and return
    return *((float*)(&u));
}

void f16tof32Arrays(float* dst, const short* src, size_t nelem, float scale, float bias) {
    const short* _src = reinterpret_cast<const short*>(src);

    for (size_t i = 0; i < nelem; i++) {
        dst[i] = f16tof32(_src[i]) * scale + bias;
    }
}
std::vector<std::string> readLabelsFromFile(const std::string& labelFileName) {
    std::vector<std::string> labels;

    std::ifstream inputFile;
    inputFile.open(labelFileName, std::ios::in);
    if (inputFile.is_open()) {
        std::string strLine;
        while (std::getline(inputFile, strLine)) {
            // trim(strLine);
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

}

#if 0
namespace V2Tiny {

const size_t NUMBER_OF_CLASSES = 20;

enum RawNetOut : size_t {
    INDEX_X = 0,
    INDEX_Y = 1,
    INDEX_W = 2,
    INDEX_H = 3,
    INDEX_SCALE = 4,
    INDEX_CLASS_PROB_BEGIN = 5,
    INDEX_CLASS_PROB_END = INDEX_CLASS_PROB_BEGIN + NUMBER_OF_CLASSES,
    INDEX_COUNT = INDEX_CLASS_PROB_END
};



const size_t K_ANCHOR_SN = 13;
constexpr size_t K_OUT_BLOB_ITEM_N = 25;
constexpr size_t K_2_DEPTHS = K_ANCHOR_SN * K_ANCHOR_SN;
constexpr size_t K_3_DEPTHS = K_2_DEPTHS * K_OUT_BLOB_ITEM_N;

bool TensorToBBox(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                  std::vector<InferenceFrame> frames, GstStructure *detection_result) {
    static const float K_ANCHOR_SCALES[] = {1.08f, 1.19f, 3.42f, 4.41f, 6.63f, 11.38f, 9.42f, 5.11f, 16.62f, 10.52f};
    std::vector<Yolo::DetectedObject> objects;
    for (const auto &blob_iter : output_blobs)
    {
        float *blob_data = nullptr;
        if (blob->GetPrecision() == InferenceBackend::OutputBlob::Precision::FP16) 
        {
            // workaround for KMB blob models
            blob_data = new float[GetUnbatchedSizeInBytes(blob, 1)];
            const short *blob_data_fp16 = (const short *)blob->GetData();
            Precision::f16tof32Arrays(blob_data, blob_data_fp16, GetUnbatchedSizeInBytes(blob, 1) / sizeof(short), 1,
                                      0);
        } 
        else 
        {
            blob_data = (float *)blob->GetData();
        }

        if (blob_data == nullptr) {
            fprintf(stderr, "Blob data pointer is null");
            return false;
        }

        float raw_netout[INDEX_COUNT];
        for (size_t k = 0; k < 5; k++) {
            float anchor_w = K_ANCHOR_SCALES[k * 2];
            float anchor_h = K_ANCHOR_SCALES[k * 2 + 1];

            for (size_t i = 0; i < K_ANCHOR_SN; i++) {
                for (size_t j = 0; j < K_ANCHOR_SN; j++) {
                    const size_t common_offset = k * K_3_DEPTHS + i * K_ANCHOR_SN + j;

                    raw_netout[INDEX_X] = blob_data[common_offset + 1 * K_2_DEPTHS];     // x
                    raw_netout[INDEX_Y] = blob_data[common_offset + 0 * K_2_DEPTHS];     // y
                    raw_netout[INDEX_W] = blob_data[common_offset + 3 * K_2_DEPTHS];     // w
                    raw_netout[INDEX_H] = blob_data[common_offset + 2 * K_2_DEPTHS];     // h
                    raw_netout[INDEX_SCALE] = blob_data[common_offset + 4 * K_2_DEPTHS]; // scale

                    for (size_t i = INDEX_CLASS_PROB_BEGIN; i < INDEX_CLASS_PROB_END; ++i) {
                        raw_netout[i] =
                            blob_data[common_offset + i * K_2_DEPTHS] * raw_netout[INDEX_SCALE]; // scaled probs
                        if (raw_netout[i] <= gva_detect->threshold)
                            raw_netout[i] = 0.f;
                    }

                    std::pair<int, float> max_info = std::make_pair(0, 0.f);
                    for (size_t l = INDEX_CLASS_PROB_BEGIN; l < INDEX_CLASS_PROB_END; l++) {
                        float class_prob = raw_netout[l];
                        if (class_prob > 1.f) {
                            printf("class_prob weired %f", class_prob);
                        }
                        if (class_prob > max_info.second) {
                            max_info.first = l - INDEX_CLASS_PROB_BEGIN;
                            max_info.second = class_prob;
                        }
                    }

                    if (max_info.second > threshold) {
                        // scale back to image width/height
                        float cx = (j + raw_netout[INDEX_X]) / K_ANCHOR_SN;
                        float cy = (i + raw_netout[INDEX_Y]) / K_ANCHOR_SN;
                        float w = std::exp(raw_netout[INDEX_W]) * anchor_w / K_ANCHOR_SN;
                        float h = std::exp(raw_netout[INDEX_H]) * anchor_h / K_ANCHOR_SN;
                        DetectedObject object(cx, cy, w, h, max_info.first, max_info.second);
                        objects.push_back(object);
                    }
                }
            }
        }
    }

    run_nms(objects, nms_threshold);

    for (DetectedObject &object : objects) {

    }

    return true;
}

} // namespace V2Tiny
#endif

namespace hva
{



void UniteHelper::setup() {

    HddlUnite::WorkloadContext::Ptr context = _workloadContextHelper.getWorkloadContext();
    _inferDataPtr = HddlUnite::Inference::makeInferData(_auxBlob, context, needPP, 10);

    _uniteGraphHelper = std::make_shared<HddlUnite_Graph_Helper>(graphName, graphPath, *context);

    // HddlUnite::WorkloadContext::Ptr context = _workloadContextHelper.getWorkloadContext();
    _workloadId = _workloadContextHelper.getWorkloadId();

    // can replaced by remoteMemoryHelper.allocateRemoteMemory()

    if (0 == _remoteMemoryFd)
    {
        _remoteMemoryPtr = HddlUnite::SMM::allocate(*context, inputSizePP);
        _remoteMemoryFd = _remoteMemoryPtr->getDmaBufFd();
    }


    auto inputBlob = _inferDataPtr->getInputBlob(inputName);
    if (inputBlob == nullptr) {
        std::cout << "Input blob not found : creating default\n";
        try {
            const int isInput = true;

            const bool isRemoteMem = true;
            const bool needAllocate = false;

            HddlUnite::Inference::BlobDesc blobDesc(precision, isRemoteMem, needAllocate);
            HddlUnite::Inference::Rectangle rect {0, 0, videoWidth, videoHeight};
            blobDesc.m_fd = _remoteMemoryFd;
            // blobDesc.m_format = HddlUnite::Inference::NV12;
            // blobDesc.m_dataSize  = videoWidthStride * videoHeight * 3/2;
            blobDesc.m_res_width = videoWidth;
            blobDesc.m_res_height = videoHeight;
            blobDesc.m_width_stride = videoWidthStride;
            blobDesc.m_plane_stride = videoWidthStride * videoHeight;
            blobDesc.m_rect.push_back(rect);
            if(!_inferDataPtr->createBlob(inputName, blobDesc, isInput))
            {
                printf("[debug] error!\n");
            }

            if (needPP)
            {
                // need pp
                // set nn input desc to inferData, only need for video workload
                // we will allocate its RemoteMemory, all parameters can be got from network parser
                HddlUnite::Inference::NNInputDesc nnInputDesc {
                    precision, isRemoteMem, true, static_cast<uint64_t>(inputSizeNN)};
                _inferDataPtr->setNNInputDesc(nnInputDesc);

                // set PP shave.
                _inferDataPtr->setPPShaveNumber(4);
            }   
        } catch (const std::exception& ex) {
            THROW_IE_EXCEPTION << "Failed to create default input blob: " << ex.what();
        }
    }

    auto outputBlob = _inferDataPtr->getOutputBlob(outputName);
    if (outputBlob == nullptr) {
        std::cout << "Output blob not found : creating default\n";
        try {
            const int isInput = false;
            const bool isRemoteMem = true;
            const bool needAllocate = true;

            HddlUnite::Inference::BlobDesc blobDesc(precision, isRemoteMem, needAllocate, outputSize);
            if (!_inferDataPtr->createBlob(outputName, blobDesc, isInput))
            {
                printf("[debug] error!\n");
            }

        } catch (const std::exception& ex) {
            THROW_IE_EXCEPTION << "Failed to create default output blob: " << ex.what();
        }
    }
}
void UniteHelper::callInferenceOnBlobs(RemoteMemoryFd remoteMemoryFd, const std::vector<InfoROI_t>& vecROI) {

    // if (_remoteMemoryPtr == nullptr) {
    //     return;
    // }

    FILE* fp;
    fp = fopen(inputPath.c_str(), "rb");

    void* ptrTemp = malloc(inputSizePP);
    fread(ptrTemp, 1, inputSizePP, fp);
    fclose(fp);

    // HddlUnite::SMM::RemoteMemory temp(*_workloadContextHelper.getWorkloadContext(), _remoteMemoryFd, inputSizePP);
    // temp.syncFromDevice(ptrTemp, inputSizePP);

    // fp = fopen("input.dat","wb");
    // fwrite(ptrTemp, 1, inputSizePP, fp);
    // fclose(fp);

    // _remoteMemoryPtr->syncFromDevice(ptrTemp, inputSizePP);
    // fp = fopen("input_from_device.dat","wb");
    // fwrite(ptrTemp, 1, inputSizePP, fp);
    // fclose(fp);

    cv::Mat frameGray;
    cv::Mat frameBGR;

    printf("[debug] videoHeight: %d, videoWidth: %d, inputSizePP: %d\n", videoHeight, videoWidth, inputSizePP);
    printf("[debug] align videoHeight: %d, align videoWidth: %d\n", alignTo<64>(videoHeight), alignTo<64>(videoWidth));
    frameGray = cv::Mat(cv::Size(alignTo<64>(videoWidth),alignTo<64>(videoHeight)*3/2), CV_8UC1);
    memcpy(frameGray.data, ptrTemp, alignTo<64>(videoWidth)*alignTo<64>(videoHeight)*3/2);

    cv::cvtColor(frameGray, frameBGR, cv::COLOR_YUV2BGR_NV12);

    free(ptrTemp);


    auto graph = _uniteGraphHelper->getGraph();
    HddlStatusCode inferStatus = HddlUnite::Inference::inferSync(*graph, _inferDataPtr);
    // HddlStatusCode inferStatus = HddlUnite::Inference::inferAsync(*graph, inferDataPtr);
    // if (inferStatus != HDDL_OK) {
    //     return;
    // }

    // inferStatus = HddlUnite::Inference::waitInfer(inferDataPtr, 10000);
    // if (inferStatus != HDDL_OK) {
    //     THROW_IE_EXCEPTION << "Failed to infer";
    // }



    auto& outputsInfo = _inferDataPtr->getOutBlobs();
    for (auto& item : outputsInfo) {
        assert(item.second != nullptr);
        auto memoryBuffer = item.second->getData();
        assert(memoryBuffer.size() > 0);

        // printf("[debug]dump file\n");

        // FILE* fp;
        // fp = fopen("output.bin", "wb");
        // fwrite(memoryBuffer.data(), 1, memoryBuffer.size(), fp);            
        // fclose(fp);

        InferenceEngine::TensorDesc desc(InferenceEngine::Precision::FP32,
            {1, 1, 1, memoryBuffer.size() / 2}, InferenceEngine::Layout::NHWC);
        
        InferenceEngine::Blob::Ptr ptrBlob = InferenceEngine::make_shared_blob<float>(desc);
        ptrBlob->allocate();
        float* ptrFP32 = ptrBlob->buffer();
        hva::f16tof32Arrays(ptrFP32, (short*)(memoryBuffer.data()), memoryBuffer.size() / 2);

        //post processing
        if ("resnet" == graphName)
        {
            std::vector<int>& vecIdx = _vecIdx;
            std::vector<std::string>& vecLabel = _vecLabel;
            std::vector<float>& vecConfidence = _vecConfidence;
            vecIdx.clear();
            vecLabel.clear();
            vecConfidence.clear();
            for (int i = 0; i < std::min(_vecROI.size(), 10ul); i++)
            {
                float* ptrFP32_ROI = ptrFP32 + outputSize / 2 * i;
                float max = 0.0f;
                float sum = 0.0f;
                int idx = 0;
                for (int j = 0; j < outputSize / 2; j++)
                {
                    sum += exp(ptrFP32_ROI[j]);
                    if (ptrFP32_ROI[j] > max)
                    {
                        idx = j;
                        max = ptrFP32_ROI[j];
                    }
                }
                vecIdx.push_back(idx);
                std::vector<std::string> labels = readLabelsFromFile("/home/kmb/cong/graph/resnet.labels");
                vecLabel.push_back(labels[idx]);
                vecConfidence.push_back(exp(max) / sum);
                printf("[debug] roi label is : %s\n", labels[idx].c_str());
            }
        }
        if ("yolotiny" == graphName)
        {
            std::vector<DetectedObject_t>& vecOjects = _vecOjects;
            vecOjects =  ::YoloV2Tiny::TensorToBBoxYoloV2TinyCommon(ptrBlob, videoHeight, videoWidth,
                                        .6, YoloV2Tiny::fillRawNetOut);

            for (auto& object : vecOjects)
            {
                printf("[debug] object detected: x is %d, y is %d, w is %d, h is %d\n", object.x, object.y, object.width, object.height);
                cv::rectangle(frameBGR, cv::Rect(object.x, object.y, object.width, object.height), cv::Scalar(0,255,0), 2);
            }
            char filename[256];
            snprintf(filename, inputPath.size() - 4, "%s", inputPath.c_str() );
            snprintf(filename + strlen(filename), 256, "%s", "output.jpg");
            static int frameCnt = 0;
            snprintf(filename, 256, "./output/debug-output-%d.jpg", frameCnt);
            frameCnt++;
            std::cout << filename << std::endl;
            // cv::imwrite(filename, frameBGR);
            // cv::imshow("output", frameBGR);
            // cv::waitKey(10);
        }


    }
}
UniteHelper::UniteHelper(std::string graphName, std::string graphPath, int32_t videoWidth, int32_t videoHeight,
                    bool needPP, int32_t inputSizeNN, int32_t outputSize, WorkloadID id, RemoteMemoryFd remoteMemoryFd):
                    _workloadContextHelper(id), graphName(graphName), graphPath(graphPath), videoWidth(videoWidth), 
                    videoHeight(videoHeight), needPP(needPP), _remoteMemoryFd(remoteMemoryFd), inputSizeNN(inputSizeNN),
                    outputSize(outputSize)
{
    if (needPP)
    {
        inputSizePP = alignTo<64>(videoWidth)*alignTo<64>(videoHeight)*3/2;
    }
    else
    {
        inputSizePP = videoWidth*videoHeight*3;
    }
    videoWidthStride = alignTo<64>(videoWidth);
}
UniteHelper::UniteHelper(WorkloadID id, std::string graphName, std::string graphPath, int32_t inputSizeNN, int32_t outputSize,
                        RemoteMemoryFd remoteMemoryFd):
                    _workloadContextHelper(id), graphName(graphName), graphPath(graphPath), _remoteMemoryFd(remoteMemoryFd),
                     inputSizeNN(inputSizeNN), outputSize(outputSize)
{

}

void UniteHelper::update(int32_t videoWidth, int32_t videoHeight, uint64_t fd, const std::vector<InfoROI_t>& vecROI)
{
    this->videoWidth = videoWidth;
    this->videoHeight = videoHeight;
    this->needPP = needPP;
    if (0ul != fd)
    {
        _remoteMemoryFd = fd;
    }

    if (needPP)
    {
        inputSizePP = alignTo<64>(videoWidth)*alignTo<64>(videoHeight)*3/2;
    }
    else
    {
        inputSizePP = videoWidth*videoHeight*3;
    }
    videoWidthStride = alignTo<64>(videoWidth); 

    auto inputBlob = _inferDataPtr->getInputBlob(inputName);

    const int isInput = true;

    const bool isRemoteMem = true;
    const bool needAllocate = false;

    HddlUnite::Inference::BlobDesc blobDesc(precision, isRemoteMem, needAllocate);
    blobDesc.m_fd = _remoteMemoryFd;
    // blobDesc.m_format = HddlUnite::Inference::NV12;
    // blobDesc.m_dataSize  = videoWidthStride * videoHeight * 3/2;
    blobDesc.m_res_width = videoWidth;
    blobDesc.m_res_height = videoHeight;
    blobDesc.m_width_stride = videoWidthStride;
    blobDesc.m_plane_stride = alignTo<64>(videoWidth) * alignTo<64>(videoHeight);


    if (vecROI.size() > 0)
    {
        for (auto& roi : vecROI)
        {
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
}