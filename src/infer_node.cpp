// #define HVA_CV
#define HVA_KMB

#include <infer_node.hpp>
#include <timeMeasure.hpp>
#include <iostream>

#include <vector>
#include <memory>
#include <string>
#include <ie_compound_blob.h>
#include "stdio.h"

#define HVA_NV12
#ifdef HVA_CV
#include "include/ocv_common.hpp"
#endif

#include "classification_results.h"
#include "tinyYolov2_post.h"
#include <ie_iextension.h>

#ifdef HVA_KMB
#include "vpusmm/vpusmm.h"
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "region_yolov2tiny.h"
#endif
using namespace InferenceEngine;

#ifdef HVA_KMB

#endif
InferNode::InferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const InferInputParams_t& params)
        :m_params(params), hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum){

}

std::shared_ptr<hva::hvaNodeWorker_t> InferNode::createNodeWorker() const{
    return std::shared_ptr<hva::hvaNodeWorker_t>(new InferNodeWorker((InferNode*)this));
}

InferNodeWorker::InferNodeWorker(hva::hvaNode_t* parentNode)
        :
        hva::hvaNodeWorker_t(parentNode), m_input_height(0), m_input_width(0){

}
#ifdef HVA_KMB
Blob::Ptr yoloLayer_yolov2tiny(const Blob::Ptr &lastBlob, int inputHeight, int inputWidth) {
    const TensorDesc quantTensor = lastBlob->getTensorDesc();
    const TensorDesc outTensor = TensorDesc(InferenceEngine::Precision::FP32,
        {1, 1, 13*13*20*5, 7},
        lastBlob->getTensorDesc().getLayout());
    Blob::Ptr outputBlob = make_shared_blob<float>(outTensor);
    outputBlob->allocate();
    memset(outputBlob->buffer(), 0, outputBlob->byteSize());
    const float *inputRawData = lastBlob->cbuffer().as<const float *>();
    float *outputRawData = outputBlob->buffer().as<PrecisionTrait<Precision::FP32>::value_type *>();

    int shape[]={13, 13, 5, 25};
    int strides[]={13*128, 128, 25, 1};
    postprocess::yolov2(inputRawData, shape, strides,
        0.4f, 0.45f, 20, 416, 416, outputRawData);

    return outputBlob;
}

// int VPUAllocator::_pageSize = getpagesize();
int VPUAllocator::_pageSize = 4 * 1024 * 1024;

static uint32_t calculateRequiredSize(uint32_t blobSize, int pageSize) {
    uint32_t blobSizeRem = blobSize % pageSize;
    uint32_t requiredSize = (blobSize / pageSize) * pageSize;
    if (blobSizeRem) {
        requiredSize += pageSize;
    }
    return requiredSize;
}

void* VPUAllocator::allocate(size_t requestedSize) {
    const uint32_t requiredBlobSize = calculateRequiredSize(requestedSize, _pageSize);
    int fileDesc = vpusmm_alloc_dmabuf(requiredBlobSize, VPUSMMTYPE_COHERENT);
    if (fileDesc < 0) {
        throw std::runtime_error("VPUAllocator::allocate: vpusmm_alloc_dmabuf failed");
    }

    unsigned long physAddr = vpusmm_import_dmabuf(fileDesc, VPU_DEFAULT);
    if (physAddr == 0) {
        throw std::runtime_error("VPUAllocator::allocate: vpusmm_import_dmabuf failed");
    }

    void* virtAddr = mmap(0, requiredBlobSize, PROT_READ|PROT_WRITE, MAP_SHARED, fileDesc, 0);
    if (virtAddr == MAP_FAILED) {
        throw std::runtime_error("VPUAllocator::allocate: mmap failed");
    }
    std::tuple<int, void*, size_t> memChunk(fileDesc, virtAddr, requiredBlobSize);
    _memChunks.push_back(memChunk);

    return virtAddr;
}

VPUAllocator::~VPUAllocator() {
    for (const std::tuple<int, void*, size_t> & chunk : _memChunks) {
        int fileDesc = std::get<0>(chunk);
        void* virtAddr = std::get<1>(chunk);
        size_t allocatedSize = std::get<2>(chunk);
        vpusmm_unimport_dmabuf(fileDesc);
        munmap(virtAddr, allocatedSize);
        close(fileDesc);
    }
}

Blob::Ptr InferNodeWorker::deQuantizeClassification(const Blob::Ptr &quantBlob, float scale, uint8_t zeroPoint) {
  const TensorDesc quantTensor = quantBlob->getTensorDesc();
  SizeVector dims = quantTensor.getDims();
  size_t batchSize = dims.at(0);
  std::cout << dims[0] << " " << dims[1] << " " << dims[2] << " " << dims[3] << std::endl;
  const size_t Count = quantBlob->size() / batchSize;
  const size_t ResultsCount = Count > 1000 ? 1000 : Count;
  dims[1] = ResultsCount;
  const TensorDesc outTensor = TensorDesc(
      InferenceEngine::Precision::FP32,
      dims,
      quantTensor.getLayout());
  std::cout << dims[0] << " " << dims[1] << " " << dims[2] << " " << dims[3] << std::endl;
  Blob::Ptr outputBlob = make_shared_blob<float>(outTensor);
  outputBlob->allocate();
  float *outRaw = outputBlob->buffer().as<PrecisionTrait<Precision::FP32>::value_type *>();
  const uint8_t *quantRaw = quantBlob->cbuffer().as<const uint8_t *>();

  for (size_t pos = 0; pos < outputBlob->size(); pos++) {
    outRaw[pos] = (quantRaw[pos] - zeroPoint) * scale;
  }
  return outputBlob;
}
Blob::Ptr InferNodeWorker::deQuantize(const Blob::Ptr &quantBlob, float scale, uint8_t zeroPoint) {
    const TensorDesc quantTensor = quantBlob->getTensorDesc();
    const TensorDesc outTensor = TensorDesc(
        InferenceEngine::Precision::FP32,
        quantTensor.getDims(),
        quantTensor.getLayout());
    const uint8_t *quantRaw = quantBlob->cbuffer().as<const uint8_t *>();

    std::vector<size_t> dims = quantTensor.getDims();

    Blob::Ptr outputBlob = make_shared_blob<float>(outTensor);
    outputBlob->allocate();
    float *outRaw = outputBlob->buffer().as<PrecisionTrait<Precision::FP32>::value_type *>();
    for (size_t pos = 0; pos < quantBlob->byteSize(); pos++) {
        outRaw[pos] = (quantRaw[pos] - zeroPoint) * scale;
    }

    return outputBlob;
}
#endif

void InferNodeWorker::init(){
    auto& param = dynamic_cast<InferNode*>(hva::hvaNodeWorker_t::getParentPtr())->m_params;
    auto& batchingConfig = hva::hvaNodeWorker_t::getParentPtr()->getBatchingConfig();

    // --------------------------- 1. Load inference engine instance -------------------------------------
    ie;
    filenameModel = param.filenameModel;
#ifndef HVA_KMB
    // ie.AddExtension(std::make_shared<Extensions::Cpu::CpuExtensions>(), "CPU");
    auto extension_ptr = make_so_pointer<IExtension>("libcpu_extension_avx2.so");
    ie.AddExtension(extension_ptr, "CPU");
    // -----------------------------------------------------------------------------------------------------

    // --------------------------- 2. Read IR Generated by ModelOptimizer (.xml and .bin files) ------------
     
    network_reader.ReadNetwork(fileNameToString(filenameModel));
    network_reader.ReadWeights(fileNameToString(filenameModel).substr(0, filenameModel.size() - 4) + ".bin");
    network_reader.getNetwork().setBatchSize(batchingConfig.batchSize);
    network = network_reader.getNetwork();
    // -----------------------------------------------------------------------------------------------------


    // --------------------------- 3. Configure input & output ---------------------------------------------
    // --------------------------- Prepare input blobs -----------------------------------------------------
    input_info = network.getInputsInfo().begin()->second;
    input_name = network.getInputsInfo().begin()->first;

    if (INFER_FORMAT_BGR == param.format) {
        /* Mark input as resizable by setting of a resize algorithm.
            * In this case we will be able to set an input blob of any shape to an infer request.
            * Resize and layout conversions are executed automatically during inference */
        input_info->getPreProcess().setResizeAlgorithm(ResizeAlgorithm::RESIZE_BILINEAR);

        input_info->setLayout(Layout::NHWC);
        input_info->setPrecision(Precision::U8);

        #if 0
        if (batchingConfig.batchSize == 1) {
            preproc = InferNodeWorker::preprocessBGR;
        } 
        else if (batchingConfig.batchSize > 1) {
            preproc = InferNodeWorker::preprocessBatchBGR;
        }
        #endif

    }
    else if (INFER_FORMAT_NV12 == param.format) {
        // set input color format to ColorFormat::NV12 to enable automatic input color format
        // pre-processing        
        input_info->setLayout(Layout::NCHW);
        input_info->setPrecision(Precision::U8);
        // set input resize algorithm to enable input autoresize
        input_info->getPreProcess().setResizeAlgorithm(ResizeAlgorithm::RESIZE_BILINEAR);
        // set input color format to ColorFormat::NV12 to enable automatic input color format
        // pre-processing
        input_info->getPreProcess().setColorFormat(ColorFormat::NV12);
        if (batchingConfig.batchSize == 1) {
            #if 0
            preproc = InferNodeWorker::preprocessNV12;
            #endif
        } 
        else if (batchingConfig.batchSize > 1) {
            printf("bathing NV12 not supported\n");
            assert(false);
        }
    }
    // --------------------------- Prepare output blobs ----------------------------------------------------
    output_info = network.getOutputsInfo().begin()->second;
    output_name = network.getOutputsInfo().begin()->first;

    output_info->setPrecision(Precision::FP32);
    // -----------------------------------------------------------------------------------------------------

    // --------------------------- 4. Loading model to the device ------------------------------------------
    std::string device_name = "CPU";
    executable_network = ie.LoadNetwork(network, device_name);
#else
    std::string device_name = "KMB";
    executable_network = ie.ImportNetwork(filenameModel, device_name, {});
    ConstInputsDataMap mapInputData = executable_network.GetInputsInfo();
    ConstOutputsDataMap mapOutputData = executable_network.GetOutputsInfo();
    InputInfo* input_info = const_cast<InputInfo*>(mapInputData.begin()->second.get());
    input_info->getPreProcess().setResizeAlgorithm(RESIZE_BILINEAR);
    input_info->getPreProcess().setColorFormat(ColorFormat::NV12);
    input_name = mapInputData.begin()->first;
    output_name = mapOutputData.begin()->first;
#endif
    // -----------------------------------------------------------------------------------------------------

    // --------------------------- 5. Create infer request -------------------------------------------------
    infer_request = executable_network.CreateInferRequest();
    // -----------------------------------------------------------------------------------------------------
    
    // set pre & post processing function
    preproc = param.preproc;
    postproc = param.postproc;
}

void InferNodeWorker::process(std::size_t batchIdx){

    vecBlobInput = hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});

    if(vecBlobInput.size()==0u)
        return;
    preproc(*this);

    // --------------------------- 7. Do inference --------------------------------------------------------
    /* Running the request synchronously */
    infer_request.Infer();
    // -----------------------------------------------------------------------------------------------------
    
    if (nullptr != postproc) {
        postproc(*this);
    }
    else {
        printf("no post proc\n");
    }
}

#if 0
//need modification to fit input
void InferNodeWorker::preprocessBGR(InferNodeWorker& inferWorker) {
    // --------------------------- 6. Prepare input --------------------------------------------------------
    /* Read input image to a blob and set it to an infer request without resize and layout conversions. */
    auto& input_name = inferWorker.input_name;
    auto& infer_request = inferWorker.infer_request;
    cv::Mat image = cv::imread("car.bmp");
    InferenceEngine::Blob::Ptr imgBlob = wrapMat2Blob(image);  // just wrap Mat data by Blob::Ptr without allocating of new memory
    infer_request.SetBlob(input_name, imgBlob);  // infer_request accepts input blob of any size
    // -----------------------------------------------------------------------------------------------------
}

//need modification to fit input
void InferNodeWorker::preprocessBatchBGR(InferNodeWorker& inferWorker) {
    auto& input_name = inferWorker.input_name;
    auto& infer_request = inferWorker.infer_request;
    cv::Mat image = cv::imread("image/car.bmp");
    auto& batchingConfig = inferWorker.getParentPtr()->getBatchingConfig();
    unsigned int batchSize = batchingConfig.batchSize;

    size_t channels = image.channels();
    size_t height = image.size().height;
    size_t width = image.size().width;

    size_t strideH = image.step.buf[0];
    size_t strideW = image.step.buf[1];

    bool is_dense =
            strideW == channels &&
            strideH == channels * width;

    if (!is_dense) THROW_IE_EXCEPTION
                << "Doesn't support conversion from not dense cv::Mat";

    printf("batch size is %d\n", batchSize);

    InferenceEngine::TensorDesc tDesc(InferenceEngine::Precision::U8,
                                      {batchSize, channels, height, width},
                                      InferenceEngine::Layout::NHWC);
    
    auto imageInput = InferenceEngine::make_shared_blob<uint8_t>(tDesc);
    imageInput->allocate();
    unsigned char* data = static_cast<unsigned char*>(imageInput->buffer());
    for (size_t n = 0; n < batchSize; n++) {
        for (size_t h = 0; h < height; h++) {
            for (size_t w = 0; w < width; w++) {
                for (size_t c = 0; c < channels; c++) {
                    data[n * height * width * channels + h * width * channels + w * channels + c] 
                        = image.data[h * width * channels + w * channels + c];
                }
            }
        }
    }
    infer_request.SetBlob(input_name, imageInput);
}
#endif

void InferNodeWorker::preprocessNV12_ROI(InferNodeWorker& inferWorker) {
    // --------------------------- 6. Prepare input --------------------------------------------------------
    /* Read input image to a blob and set it to an infer request without resize and layout conversions. */
    auto& input_name = inferWorker.input_name;
    auto& infer_request = inferWorker.infer_request;

    auto& vecBlobInput = inferWorker.vecBlobInput;
    auto& pBlob = vecBlobInput[0];
    auto& blob = *pBlob; 
    auto pBufROI = blob.get<unsigned char, InfoROI_t>(0);
    unsigned char* roi_buf = pBufROI->getPtr();
    InfoROI_t* pInfoROI = pBufROI->getMeta();
    const size_t roi_height = pInfoROI->height;
    const size_t roi_width = pInfoROI->width;

    auto pBufImage = blob.get<unsigned char, std::pair<unsigned, unsigned>>(1);
    unsigned char* image_buf = pBufImage->getPtr();
    std::pair<unsigned, unsigned>* metaImage = pBufImage->getMeta();
    const size_t image_height = metaImage->second;
    const size_t image_width = metaImage->first;
    
#ifdef HVA_CV
    auto& picBGR = inferWorker.picBGR;
    picBGR = cv::Mat(roi_height, roi_width, CV_8UC3, roi_buf);
    // InferenceEngine::Blob::Ptr imgBlob = wrapMat2Blob(picBGR);  // just wrap Mat data by Blob::Ptr without allocating of new memory
#endif

    InferenceEngine::TensorDesc planeY(InferenceEngine::Precision::U8,
        {1, 1, image_height, image_width}, InferenceEngine::Layout::NHWC);
    InferenceEngine::TensorDesc planeUV(InferenceEngine::Precision::U8,
        {1, 2, image_height / 2, image_width / 2}, InferenceEngine::Layout::NHWC);
    const size_t offset = image_height * image_width;

    Blob::Ptr blobY = make_shared_blob<uint8_t>(planeY, image_buf);
    Blob::Ptr blobUV = make_shared_blob<uint8_t>(planeUV, image_buf + offset);

    struct ROI_t
    {
        ROI_t(int x, int y, int width, int height):
        x(x), y(y), width(width), height(height)
         {};
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };
    ROI_t roi({
        .x = pInfoROI->x,
        .y = pInfoROI->y,
        .width = pInfoROI->width,
        .height = pInfoROI->height,
    });
    std::cout << roi.x << ", " << roi.y << ", " << roi.width << ", " << roi.height << std::endl;
    InferenceEngine::ROI crop_roi_y(
    {0,
        (size_t) ((roi.x & 0x1) ? roi.x - 1 : roi.x),
        (size_t) ((roi.y & 0x1) ? roi.y - 1 : roi.y),
        (size_t) ((roi.width & 0x1) ? roi.width - 1 : roi.width),
        (size_t) ((roi.height & 0x1) ? roi.height - 1 : roi.height),
    }
    );

    InferenceEngine::ROI crop_roi_uv(
    {0,
        (size_t)roi.x / 2,
        (size_t)roi.y / 2,
        (size_t)roi.width / 2,
        (size_t)roi.height / 2
    }
    );
    InferenceEngine::Blob::Ptr y_plane_with_roi = InferenceEngine::make_shared_blob(blobY, crop_roi_y);
    InferenceEngine::Blob::Ptr uv_plane_with_roi = InferenceEngine::make_shared_blob(blobUV, crop_roi_uv);

    Blob::Ptr nv12Blob = make_shared_blob<NV12Blob>(y_plane_with_roi, uv_plane_with_roi);

    infer_request.SetBlob(input_name, nv12Blob);  // infer_request accepts input blob of any size
    // -----------------------------------------------------------------------------------------------------
}
#ifdef HVA_CV
void InferNodeWorker::preprocessBGR_ROI(InferNodeWorker& inferWorker) {
    // --------------------------- 6. Prepare input --------------------------------------------------------
    /* Read input image to a blob and set it to an infer request without resize and layout conversions. */
    auto& input_name = inferWorker.input_name;
    auto& infer_request = inferWorker.infer_request;

    auto& vecBlobInput = inferWorker.vecBlobInput;
    auto& pBlob = vecBlobInput[0];
    auto& blob = *pBlob; 
    auto pBuf = blob.get<unsigned char, InfoROI_t>(0);
    unsigned char* image_buf = pBuf->getPtr();
    InfoROI_t* meta = pBuf->getMeta();
    const size_t input_height = meta->height;
    const size_t input_width = meta->width;

    auto& picBGR = inferWorker.picBGR;
    picBGR = cv::Mat(input_height, input_width, CV_8UC3, image_buf);

    InferenceEngine::Blob::Ptr imgBlob = wrapMat2Blob(picBGR);  // just wrap Mat data by Blob::Ptr without allocating of new memory
    infer_request.SetBlob(input_name, imgBlob);  // infer_request accepts input blob of any size
    // -----------------------------------------------------------------------------------------------------
}
#endif
//need modification to fit input
void InferNodeWorker::postprocessClassification(InferNodeWorker& inferWorker) {
    // --------------------------- 8. Process output ------------------------------------------------------
    auto& output_name = inferWorker.output_name;
    auto& infer_request = inferWorker.infer_request;

    auto& batchingConfig = inferWorker.getParentPtr()->getBatchingConfig();
    unsigned int batchSize = batchingConfig.batchSize;

    InferenceEngine::Blob::Ptr output = infer_request.GetBlob(output_name);
    auto desc = output->getTensorDesc();
    auto dims = desc.getDims();
    float* data = static_cast<float*>(output->buffer());
#ifdef HVA_KMB
    output = InferNodeWorker::deQuantizeClassification(output, 1, 0);
#endif
    // Print classification results
    std::vector<std::string> filenames;
    auto& vecBlobInput = inferWorker.vecBlobInput;
    std::string str = std::to_string(vecBlobInput[0]->frameId);
    for (int i = 0; i < batchSize; i++){
        filenames.push_back(str);
    }

    ClassificationResult classificationResult(output, filenames, batchSize);
    classificationResult.print();

#ifdef HVA_CV
    auto& picBGR = inferWorker.picBGR;

    cv::imshow("classify", picBGR);
    cv::waitKey(10);
#endif

    // -----------------------------------------------------------------------------------------------------
}


void InferNodeWorker::postprocessTinyYolov2(InferNodeWorker& inferWorker) {

    // --------------------------- 8. Process output ------------------------------------------------------
    auto& output_name = inferWorker.output_name;
    auto& infer_request = inferWorker.infer_request;

    auto& batchingConfig = inferWorker.getParentPtr()->getBatchingConfig();
    unsigned int batchSize = batchingConfig.batchSize;

#ifdef HVA_CV
    auto& picNV12 = inferWorker.picNV12;
#endif
    const InferenceEngine::Blob::Ptr output_blob = infer_request.GetBlob(output_name);

#ifndef HVA_KMB
    std::vector<DetectedObject_t> vecObjects = YoloV2Tiny::TensorToBBoxYoloV2TinyCommon(output_blob, inferWorker.m_input_height, inferWorker.m_input_width, 0.5, YoloV2Tiny::fillRawNetOut);
#else
    auto desc = output_blob->getTensorDesc();
    auto dims = desc.getDims();
    float* data = static_cast<float*>(output_blob->buffer());
    Blob::Ptr dequantOut = InferNodeWorker::deQuantize(output_blob, 0.33713474f, 221);

    FILE* fp = fopen("dumpOutput.bin", "wb");
    fwrite((unsigned char*)data, 1, output_blob->byteSize(), fp);
    fclose(fp);

    // Region YOLO layer
    // const int imageWidth = 1080;
    // const int imageHeight = 720;
    Blob::Ptr detectResult = yoloLayer_yolov2tiny(dequantOut, inferWorker.m_input_height, inferWorker.m_input_width);

    std::vector<DetectedObject_t> vecObjects;

    // Print result.
    size_t N = detectResult->getTensorDesc().getDims()[2];
    if (detectResult->getTensorDesc().getDims()[3] != 7) {
        throw std::logic_error("Output item should have 7 as a last dimension");
    }
    const float *rawData = detectResult->cbuffer().as<const float *>();
    
    // imageid,labelid,confidence,x0,y0,x1,y1
    for (size_t i = 0; i < N; i++) {
        if (rawData[i*7 + 2] > 0.001) 
        {
            DetectedObject_t object;
            object.x = (rawData[i*7 + 3]) * inferWorker.m_input_width / 416;
            object.y = (rawData[i*7 + 4]) * inferWorker.m_input_height / 416;
            object.width = (rawData[i*7 + 5] - rawData[i*7 + 3]) * inferWorker.m_input_width / 416;
            object.height = (rawData[i*7 + 6] - rawData[i*7 + 4]) * inferWorker.m_input_height / 416;
            object.confidence = rawData[i*7 + 2];
            vecObjects.push_back(object);

            std::cout << "confidence = " << rawData[i*7 + 2] << std::endl;
            std::cout << "x0,y0,x1,y1 = " << rawData[i*7 + 3] << ", "
                << rawData[i*7 + 4] << ", "
                << rawData[i*7 + 5] << ", "
                << rawData[i*7 + 6] << std::endl;
        }
    }        
#endif
    for(auto& object : vecObjects)
    {
        std::cout<<"Object detected:\tx\ty\tw\th"<<std::endl;
        std::cout<<"\t\t\t"<<object.x <<"\t"<<object.y<<"\t"<<object.width<<"\t"<<object.height<<"\n"<<std::endl;
    }

#ifdef HVA_CV
    //display
    auto& picBGR = inferWorker.picBGR;
    cv::cvtColor(picNV12, picBGR, CV_YUV2BGR_NV12);

    for(auto& object : vecObjects)
    {
        cv::rectangle(picBGR, cv::Rect(object.x,object.y, object.width, object.height), cv::Scalar(0,255,0));
    }

    cv::imshow("detection",picBGR);
    cv::waitKey(10);
    // -----------------------------------------------------------------------------------------------------
#endif 
}
void InferNodeWorker::postprocessTinyYolov2WithClassify(InferNodeWorker& inferWorker) {

    // --------------------------- 8. Process output ------------------------------------------------------
    auto& output_name = inferWorker.output_name;
    auto& infer_request = inferWorker.infer_request;

    auto& batchingConfig = inferWorker.getParentPtr()->getBatchingConfig();
    unsigned int batchSize = batchingConfig.batchSize;

    const InferenceEngine::Blob::Ptr output_blob = infer_request.GetBlob(output_name);
    
#ifdef HVA_CV
    auto& picNV12 = inferWorker.picNV12;
#endif
    const size_t input_height = inferWorker.m_input_height / 3 * 2;
    const size_t input_width = inferWorker.m_input_width;

#ifndef HVA_KMB
    std::vector<DetectedObject_t> vecObjects = YoloV2Tiny::TensorToBBoxYoloV2TinyCommon(output_blob, inferWorker.m_input_height, inferWorker.m_input_width, 0.5, YoloV2Tiny::fillRawNetOut);
#else
    auto desc = output_blob->getTensorDesc();
    auto dims = desc.getDims();
    float* data = static_cast<float*>(output_blob->buffer());
    Blob::Ptr dequantOut = InferNodeWorker::deQuantize(output_blob, 0.33713474f, 221);

    // Region YOLO layer
    // const int imageWidth = 1080;
    // const int imageHeight = 720;
    Blob::Ptr detectResult = yoloLayer_yolov2tiny(dequantOut, inferWorker.m_input_height, inferWorker.m_input_width);

    std::vector<DetectedObject_t> vecObjects;

    // Print result.
    size_t N = detectResult->getTensorDesc().getDims()[2];
    if (detectResult->getTensorDesc().getDims()[3] != 7) {
        throw std::logic_error("Output item should have 7 as a last dimension");
    }
    const float *rawData = detectResult->cbuffer().as<const float *>();
    
    // imageid,labelid,confidence,x0,y0,x1,y1
    for (size_t i = 0; i < N; i++) {
        if (rawData[i*7 + 2] > 0.001) {
            DetectedObject_t object;
            object.x = (rawData[i*7 + 3]) * inferWorker.m_input_width / 416;
            object.y = (rawData[i*7 + 4]) * inferWorker.m_input_height / 416;
            object.width = (rawData[i*7 + 5] - rawData[i*7 + 3]) * inferWorker.m_input_width / 416;
            object.height = (rawData[i*7 + 6] - rawData[i*7 + 4]) * inferWorker.m_input_height / 416;
            object.confidence = rawData[i*7 + 2];
            vecObjects.push_back(object);

            std::cout << "confidence = " << rawData[i*7 + 2] << std::endl;
            std::cout << "x0,y0,x1,y1 = " << rawData[i*7 + 3] << ", "
                << rawData[i*7 + 4] << ", "
                << rawData[i*7 + 5] << ", "
                << rawData[i*7 + 6] << std::endl;
        }
    }        
#endif


#ifdef HVA_CV
    auto& picBGR = inferWorker.picBGR;
    cv::cvtColor(picNV12, picBGR, CV_YUV2BGR_NV12);
#endif
    auto& vecBlobInput = inferWorker.vecBlobInput;

    //send output to classification
    for(int i = 0; i < vecObjects.size(); i++)
    {
        auto& object = vecObjects[i];
        
        InfoROI_t* pInfoROI(new InfoROI_t);
        InfoROI_t& infoROI = *pInfoROI;
        infoROI.height = inferWorker.m_input_height;
        infoROI.width = inferWorker.m_input_width;
        infoROI.width = object.width;
        infoROI.height = object.height;
        infoROI.x = object.x;
        infoROI.y = object.y;
        infoROI.indexROI = i;
        infoROI.totalROINum = vecObjects.size();
        infoROI.frameId = vecBlobInput[0]->frameId;

        unsigned char* imgBuf = new unsigned char[(unsigned)(infoROI.width * infoROI.height * 3)]; //need to be freed after classification

#ifdef HVA_CV        
        for (std::size_t ii = 0; ii < infoROI.height; ii++)
        {
            memcpy(imgBuf + ii * infoROI.width * 3, picBGR.data + (ii + object.y) * input_width * 3 + object.x * 3, infoROI.width * 3);
        }
#endif
        std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
        
        auto& vecBlobInput = inferWorker.vecBlobInput;
        blob->emplace<unsigned char, InfoROI_t>(imgBuf, (unsigned)(infoROI.width * infoROI.height * 3),
                pInfoROI, [](unsigned char* mem, InfoROI_t* meta){
                    if (nullptr != mem) {
                        delete[] mem;
                    } 
                    delete meta;
                });
        blob->push(vecBlobInput[0]->get<unsigned char, std::pair<unsigned, unsigned>>(0));
        blob->frameId = vecBlobInput[0]->frameId;
        inferWorker.sendOutput(blob,0);
    }

#ifdef HVA_CV
    //display
    for(auto& object : vecObjects)
    {
        cv::rectangle(picBGR, cv::Rect(object.x,object.y, object.width, object.height), cv::Scalar(0,255,0));
    }

    cv::imshow("detection",picBGR);
    cv::waitKey(10);
#endif
    // -----------------------------------------------------------------------------------------------------
}

void InferNodeWorker::preprocessNV12(InferNodeWorker& inferWorker) {

    // --------------------------- 6. Prepare input --------------------------------------------------------
    /* Read input image to a blob and set it to an infer request without resize and layout conversions. */
    auto& input_name = inferWorker.input_name;
    auto& infer_request = inferWorker.infer_request;
        
// --------------------------- Create a blob to hold the NV12 input data -------------------------------
    // Create tensor descriptors for Y and UV blobs
    
    auto& vecBlobInput = inferWorker.vecBlobInput;
    auto pBuf = vecBlobInput[0]->get<unsigned char, std::pair<unsigned, unsigned>>(0);

    inferWorker.m_image_buf = pBuf->getPtr();

    std::pair<unsigned,unsigned>* meta = pBuf->getMeta();
    inferWorker.m_input_height = meta->second;
    inferWorker.m_input_width = meta->first;

    inferWorker.m_input_width = ((inferWorker.m_input_width - 1) & (~15)) + 16;

    std::cout << inferWorker.m_input_height << ", " << inferWorker.m_input_width << std::endl;

    const size_t expectedSize = (inferWorker.m_input_height*inferWorker.m_input_width * 3 / 2);
 
    // inferWorker.m_image_buf = reinterpret_cast<uint8_t *>(inferWorker.allocator.allocate(expectedSize));
    FILE* fp = fopen("26.nv12", "rb");
    // fread(inferWorker.m_image_buf, 1, inferWorker.m_input_height*inferWorker.m_input_width*3/2, fp);
    fclose(fp);

    InferenceEngine::TensorDesc y_plane_desc(InferenceEngine::Precision::U8,
        {1, 1, inferWorker.m_input_height, inferWorker.m_input_width}, InferenceEngine::Layout::NHWC);
    InferenceEngine::TensorDesc uv_plane_desc(InferenceEngine::Precision::U8,
        {1, 2, inferWorker.m_input_height / 2, inferWorker.m_input_width / 2}, InferenceEngine::Layout::NHWC);
    const size_t offset = inferWorker.m_input_width * inferWorker.m_input_height;
    const size_t sizeY = offset;
    const size_t sizeUV = sizeY / 2;
    const size_t sizeYUV = sizeY + sizeUV;

    // Create blob for Y plane from raw data
    InferenceEngine::Blob::Ptr y_blob = InferenceEngine::make_shared_blob<uint8_t>(y_plane_desc, inferWorker.m_image_buf);
    // Create blob for UV plane from raw data
    InferenceEngine::Blob::Ptr uv_blob = InferenceEngine::make_shared_blob<uint8_t>(uv_plane_desc, inferWorker.m_image_buf + offset + 48 * inferWorker.m_input_width);
    
    // Create NV12Blob from Y and UV blobs
    InferenceEngine::Blob::Ptr input = make_shared_blob<InferenceEngine::NV12Blob>(y_blob, uv_blob);

#ifdef HVA_CV
    inferWorker.picNV12 = cv::Mat(inferWorker.m_input_height * 3/2, inferWorker.m_input_width, CV_8UC1);
    memcpy(inferWorker.picNV12.data, y_blob->buffer(), inferWorker.m_input_height * inferWorker.m_input_width);
    memcpy(inferWorker.picNV12.data + inferWorker.m_input_height * inferWorker.m_input_width, uv_blob->buffer(), inferWorker.m_input_height * inferWorker.m_input_width / 2);
#endif
    // --------------------------- Set the input blob to the InferRequest ----------------------------------
    infer_request.SetBlob(input_name, input);
    // -----------------------------------------------------------------------------------------------------

    fp = fopen("dumpInput.bin", "wb");
    fwrite(inferWorker.m_image_buf, 1, inferWorker.m_input_height*inferWorker.m_input_width*3/2, fp);
    fclose(fp);    
}

#if 0 //batching NV12 not supported
void InferNodeWorker::preprocessBatchNV12(InferNodeWorker& inferWorker) {
 
    //batching not supported for NV12

    // --------------------------- 6. Prepare input --------------------------------------------------------
    /* Read input image to a blob and set it to an infer request without resize and layout conversions. */
    auto& input_name = inferWorker.input_name;
    auto& infer_request = inferWorker.infer_request;
        
    auto& batchingConfig = inferWorker.getParentPtr()->getBatchingConfig();
    unsigned int batchSize = batchingConfig.batchSize;
// --------------------------- Create a blob to hold the NV12 input data -------------------------------
    // Create tensor descriptors for Y and UV blobs
    const size_t input_height = 720;
    const size_t input_width = 1080;
    
    InferenceEngine::TensorDesc y_plane_desc(InferenceEngine::Precision::U8,
        {batchSize, 1, input_height, input_width}, InferenceEngine::Layout::NHWC);
    InferenceEngine::TensorDesc uv_plane_desc(InferenceEngine::Precision::U8,
        {batchSize, 2, input_height / 2, input_width / 2}, InferenceEngine::Layout::NHWC);
    const size_t offset = input_width * input_height;
    const size_t sizeY = offset;
    const size_t sizeUV = sizeY / 2;
    const size_t sizeYUV = sizeY + sizeUV;


    FILE* fp = fopen("/home/zhangcong/hva/hva_decode_node/build/image/26.nv12", "rb");
    unsigned char* image_buf = (unsigned char*)(malloc(batchSize * sizeYUV));
    // printf("size:%d,fp:%x,buf:%x\n", sizeYUV, fp, image_buf);
    
    // fread(image_buf, sizeof(unsigned char), sizeYUV, fp);
    for (int i = 0; i < batchSize; i++){
        fseek(fp, 0, SEEK_SET);
        fread(image_buf + sizeY * i, sizeof(unsigned char), sizeY, fp);
        fread(image_buf + sizeY * batchSize + sizeUV * i, sizeof(unsigned char), sizeUV, fp);
    }

    fclose(fp);
    // Create blob for Y plane from raw data
    InferenceEngine::Blob::Ptr y_blob = InferenceEngine::make_shared_blob<uint8_t>(y_plane_desc, image_buf);
    // Create blob for UV plane from raw data
    InferenceEngine::Blob::Ptr uv_blob = InferenceEngine::make_shared_blob<uint8_t>(uv_plane_desc, image_buf + offset*batchSize);
    
    // Create NV12Blob from Y and UV blobs
    InferenceEngine::Blob::Ptr input = make_shared_blob<InferenceEngine::NV12Blob>(y_blob, uv_blob);
    
    // --------------------------- Set the input blob to the InferRequest ----------------------------------
    infer_request.SetBlob(input_name, input);
    // -----------------------------------------------------------------------------------------------------
}
#endif