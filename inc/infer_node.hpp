#include <hvaPipeline.hpp>

#include <string>

#include <inference_engine.hpp>
#include <ie_utils.hpp>
// #include <pipeline_decode.h>

enum InferFormat_t {
    INFER_FORMAT_BGR = 0,
    INFER_FORMAT_NV12
};
class InferNodeWorker;
using InferPostprocFunc = std::function<void (InferNodeWorker& inferWorker)>;

struct InferInputParams {
    std::string filenameModel;
    InferFormat_t format = INFER_FORMAT_BGR;
    InferPostprocFunc postproc = nullptr;
};

struct DetectedObject {
    int x;
    int y;
    int width;
    int height;
    float confidence;
    explicit DetectedObject(float x, float y, float h, float w, float confidence, float h_scale = 1.f,
                            float w_scale = 1.f)
        : x(static_cast<int>((x - w / 2) * w_scale)), y(static_cast<int>((y - h / 2) * h_scale)),
        width(static_cast<int>(w * w_scale)), height(static_cast<int>(h * h_scale)), confidence(confidence) {
    }
    DetectedObject() = default;
    ~DetectedObject() = default;
    DetectedObject(const DetectedObject &) = default;
    DetectedObject(DetectedObject &&) = default;
    DetectedObject &operator=(const DetectedObject &) = default;
    DetectedObject &operator=(DetectedObject &&) = default;
    bool operator<(const DetectedObject &other) const {
        return this->confidence > other.confidence; //TODO fix me
    }
};

class InferNode : public hva::hvaNode_t{
public:
    InferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const InferInputParams& params);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

    InferInputParams m_params;
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
 
    static void postprocessClassification(InferNodeWorker& inferWorker);
    static void postprocessTinyYolov2(InferNodeWorker& inferWorker);
    
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

    using InferPreprocFunc = std::function<void (InferNodeWorker& inferWorker)>;
    InferPreprocFunc preproc = nullptr;
    InferPostprocFunc postproc = nullptr;

    std::size_t m_input_height;
    std::size_t m_input_width;
};

