#ifndef HDDL2PLUGIN_HELPER_HPP
#define HDDL2PLUGIN_HELPER_HPP

#include <condition_variable>
#include <queue>

#include <inference_engine.hpp>
#include <HddlUnite.h>

#include "common.hpp"
#include "ImageNetLabels.hpp"

namespace IE = InferenceEngine;

/**
 *  Wrapper class of HDDL2 plugin inference API
 */
class HDDL2pluginHelper_t
{
public:
    using PostprocPtr_t = std::function<void(HDDL2pluginHelper_t *, IE::Blob::Ptr, std::vector<ROI> &vecROI)>;
    using InferCallback_t = std::function<void(void)>;

    HDDL2pluginHelper_t() = default;
    ~HDDL2pluginHelper_t() = default;
    HDDL2pluginHelper_t(const HDDL2pluginHelper_t &) = delete;
    HDDL2pluginHelper_t &operator=(const HDDL2pluginHelper_t &) = delete;

    /**
     * @brief Constructor of HDDL2pluginHelper_t
     * @param graphPath File path of graph file
     * @param id Workload id
     * @param ptrPostproc Postprocess function pointer
     * @param thresholdDetection Threshold of detection confidence
     * @param heightInput Height of input image
     * @param widthInput Width of input image
     */
    HDDL2pluginHelper_t(std::string graphPath, WorkloadID id = 0, PostprocPtr_t ptrPostproc = nullptr, float thresholdDetection = 0.6f,
                        size_t heightInput = 0, size_t widthInput = 0)
        : _graphPath{graphPath},
          _workloadId{id},
          _heightInput{heightInput},
          _widthInput{widthInput},
          _ptrPostproc{ptrPostproc},
          _thresholdDetection{thresholdDetection}
    {
    }
    /**
     * @brief Setup HDDL2 plugin for ImagePipe line, load network, create infer request
     * @param numInferRequest Number of infer request
     */
	void setupImgPipe(int32_t numInferRequest = 1);
    /**
     * @brief Start ImagePipe line async infer request
     * @param ptrInferRequest Pointer to infer request
     * @param callback Callback function pointer
     * @param remoteMemoryFd FD of remote memory on device
     * @param heightInput Height of input image
     * @param widthInput Width of input image
     * @param roi Inference ROI
     */
    void inferAsyncImgPipe(IE::InferRequest::Ptr ptrInferRequest, InferCallback_t callback,
                char* hostFd, size_t heightInput = 0, size_t widthInput = 0, const ROI &roi = ROI{});

    /**
     * @brief Setup HDDL2 plugin, load network, create infer request
     * @param numInferRequest Number of infer request
     */
    void setup(int32_t numInferRequest = 1);

    /**
     * @brief Start async infer request
     * @param ptrInferRequest Pointer to infer request
     * @param callback Callback function pointer
     * @param remoteMemoryFd FD of remote memory on device
     * @param heightInput Height of input image
     * @param widthInput Width of input image
     * @param roi Inference ROI
     */
    void inferAsync(IE::InferRequest::Ptr ptrInferRequest, InferCallback_t callback,
                           int remoteMemoryFd, size_t heightInput, size_t widthInput, const ROI &roi = ROI{});
    /**
     * @brief Get output blob from infer request
     * @param ptrInferRequest Pointer to infer request
     * @return Pointer to output blob
     */
    IE::Blob::Ptr getOutputBlob(IE::InferRequest::Ptr ptrInferRequest);
    
    /**
     * @brief Get idle infer request
     * @return Pointer to infer request
     */
    IE::InferRequest::Ptr getInferRequest();
    
    /**
     * @brief Put back used infer request
     * @param ptrInferRequest Pointer to infer request
     */
    void putInferRequest(IE::InferRequest::Ptr ptrInferRequest);

    /**
     * @brief Postprocess after inference
     * @param ptrBlob Pointer to inference output blob
     * @param vecROI Reference of output ROI vector
     */
    void postproc(IE::Blob::Ptr ptrBlob, std::vector<ROI> &vecROI);

    void postprocYolotinyv2_fp16(IE::Blob::Ptr ptrBlob, std::vector<ROI> &vecROI);
;
    void postprocYolotinyv2_u8(IE::Blob::Ptr ptrBlob, std::vector<ROI> &vecROI);

    void postprocResnet50_fp16(IE::Blob::Ptr ptrBlob, std::vector<ROI> &vecROI);

    void postprocResnet50_u8(IE::Blob::Ptr ptrBlob, std::vector<ROI> &vecROI);

    template <int N>
    static int alignTo(int x)
    {
        return ((x + N - 1) & ~(N - 1));
    }

    /**
     * @brief Compile xml IR graph and save .blob graph
     * @param File Path to xml IR graph 
     */
    static void compile(const std::string &graphName);

    /**
     * @brief Dequantize U8 blob to FP32 blob
     * @param quantBlob Input quantized blob
     * @param scale Quantize scale
     * @param zeroPoint Quantize zero point
     * @return Output dequantied blob
     */
    static IE::Blob::Ptr deQuantize(const IE::Blob::Ptr &quantBlob, float scale, uint8_t zeroPoint);

    /**
     * @brief Yolo region layer for output
     * @param lastBlob Last layer blob from inference
     * @param inputHeight Height of input image
     * @param inputWidth Width of input image
     * @return Output detection result
     */
    static IE::Blob::Ptr yoloLayer_yolov2tiny(const IE::Blob::Ptr &lastBlob, int inputHeight, int inputWidth);

    /**
     * @brief Read classification labels from label file
     * @param labelFileName Input file path of label file
     */
    static std::vector<std::string> readLabelsFromFile(const std::string &labelFileName);

public:
    /**
     * Infer request pool
     */
    class InferRequestPool_t
    {
    public:
        InferRequestPool_t() = delete;

        /**
         * @brief Constructor of infer request pool
         * @param executableNetwork IE executable network
         * @param numInferRequest Infer request pool size
         */
        explicit InferRequestPool_t(IE::ExecutableNetwork &executableNetwork, int32_t numInferRequest);
        ~InferRequestPool_t();

        InferRequestPool_t(const InferRequestPool_t &) = delete;
        InferRequestPool_t &operator=(const InferRequestPool_t &) = delete;

    private:
        using Type = IE::InferRequest::Ptr;

    public:
        /**
         * @brief Get idle infer request pointer
         * @return Infer request pointer
         */
        Type get();

        /**
         * @brief Put back used infer request pointer
         * @param ptrInferRequest Infer Request pointer
         */
        void put(const Type ptrInferRequest);

    private:
        bool push(Type value);

        bool pop(Type &value);

        bool empty() const;

        void close();

    private:
        mutable std::mutex _mutex;
        std::queue<Type> _queue;
        std::condition_variable _cv_empty;
        std::condition_variable _cv_full;
        std::size_t _maxSize; //max queue size
        bool _close{false};

    private:
        std::vector<Type> _vecInferRequest;

        int32_t _cntInferRequest{0};

    public:
        using Ptr = std::shared_ptr<InferRequestPool_t>;
    };

    /**
     * Class used to keep process order in frame id sequence
     */
    class OrderKeeper_t
    {
    public:
        OrderKeeper_t() = default;
        ~OrderKeeper_t() = default;

        OrderKeeper_t(const OrderKeeper_t &) = delete;
        OrderKeeper_t &operator=(const OrderKeeper_t &) = delete;

        /**
         * @brief Wait in order and lock, must be called in pair with unlock
         * @param id Frame id
         */
        void lock(uint64_t id);

        /**
         * @brief Unlock, must be called in pair with lock
         * @param id Frame id
         */
        void unlock(uint64_t id);

        /**
         * @brief Bypass the order keeper, called when order keeper lock need to be bypassed
         * @param id Frame id
         */
        void bypass(uint64_t id);

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
    InferRequestPool_t::Ptr _ptrInferRequestPool;

    std::string _inputName;

    WorkloadID _workloadId{0ul};

    std::string _graphPath;
    size_t _heightInput{0};
    size_t _widthInput{0};

    float _thresholdDetection{0.6f};

    PostprocPtr_t _ptrPostproc{nullptr};
    static ImageNetLabels m_labels;
};

#endif //#ifndef HDDL2PLUGIN_HELPER_HPP
