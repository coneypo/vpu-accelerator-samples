#include <hvaPipeline.hpp>

#include <string>
#ifdef HVA_CV
#include "opencv2/opencv.hpp"
#endif
#include <inference_engine.hpp>
#include <ie_utils.hpp>
// #include <pipeline_decode.h>

enum InferFormat_t {
    INFER_FORMAT_BGR = 0,
    INFER_FORMAT_NV12
};
class InferNodeWorker;
using InferPreprocFunc_t = std::function<void (InferNodeWorker& inferWorker)>;
using InferPostprocFunc_t = std::function<void (InferNodeWorker& inferWorker)>;

#ifdef HVA_KMB
class VPUAllocator {
public:
    VPUAllocator() {}
    virtual ~VPUAllocator();
    void* allocate(size_t requestedSize);
private:
    std::list< std::tuple<int, void*, size_t> > _memChunks;
    static int _pageSize;
};
#endif

struct InferInputParams_t {
    std::string filenameModel;
    InferFormat_t format = INFER_FORMAT_BGR;
    InferPostprocFunc_t postproc = nullptr;
    InferPreprocFunc_t preproc = nullptr;
};

struct DetectedObject_t {
    int x;
    int y;
    int width;
    int height;
    float confidence;
    explicit DetectedObject_t(float x, float y, float h, float w, float confidence, float h_scale = 1.f,
                            float w_scale = 1.f)
        : x(static_cast<int>((x - w / 2) * w_scale)), y(static_cast<int>((y - h / 2) * h_scale)),
        width(static_cast<int>(w * w_scale)), height(static_cast<int>(h * h_scale)), confidence(confidence) {
    }
    DetectedObject_t() = default;
    ~DetectedObject_t() = default;
    DetectedObject_t(const DetectedObject_t &) = default;
    DetectedObject_t(DetectedObject_t &&) = default;
    DetectedObject_t &operator=(const DetectedObject_t &) = default;
    DetectedObject_t &operator=(DetectedObject_t &&) = default;
    bool operator<(const DetectedObject_t &other) const {
        return this->confidence > other.confidence; //TODO fix me
    }
};

struct InfoROI_t {
    int widthImage = 0;
    int heightImage = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int indexROI = 0;
    int totalROINum = 0;
    int frameId = 0;
};

class InferNode : public hva::hvaNode_t{
public:
    InferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const InferInputParams_t& params);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

    InferInputParams_t m_params;
};

class InferNodeWorker : public hva::hvaNodeWorker_t{
public:
    InferNodeWorker(hva::hvaNode_t* parentNode);

    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

    static void preprocessBGR(InferNodeWorker& inferWorker);
    static void preprocessBatchBGR(InferNodeWorker& inferWorker);
    static void preprocessBatchNV12(InferNodeWorker& inferWorker);
    static void preprocessNV12(InferNodeWorker& inferWorker);
    static void preprocessBGR_ROI(InferNodeWorker& inferWorker);
    static void preprocessNV12_ROI(InferNodeWorker& inferWorker);

    static void postprocessClassification(InferNodeWorker& inferWorker);
    static void postprocessTinyYolov2WithClassify(InferNodeWorker& inferWorker);
    static void postprocessTinyYolov2(InferNodeWorker& inferWorker);

#ifdef HVA_KMB
    static InferenceEngine::Blob::Ptr deQuantize(const InferenceEngine::Blob::Ptr &quantBlob, float scale, uint8_t zeroPoint);
    static InferenceEngine::Blob::Ptr deQuantizeClassification(const InferenceEngine::Blob::Ptr &quantBlob, float scale, uint8_t zeroPoint);
#endif

private:

    std::vector<std::shared_ptr<hva::hvaBlob_t>> vecBlobInput;

    InferenceEngine::Core ie;
    InferenceEngine::CNNNetReader network_reader;
    InferenceEngine::CNNNetwork network;
    InferenceEngine::ExecutableNetwork executable_network;
    InferenceEngine::InferRequest infer_request;

    InferenceEngine::InputInfo::Ptr input_info;
    InferenceEngine::DataPtr output_info;

    std::string filenameModel;
    std::string input_name;
    std::string output_name;

    std::size_t m_input_height;
    std::size_t m_input_width;
    unsigned char* m_image_buf;

    InferPreprocFunc_t preproc = nullptr;
    InferPostprocFunc_t postproc = nullptr;
#ifdef HVA_KMB
    VPUAllocator allocator;
#endif
#ifdef HVA_CV

    cv::VideoWriter wrt;
    cv::Mat picNV12; //temporary use for display
    cv::Mat picBGR; //temporary use for display
#endif
};

