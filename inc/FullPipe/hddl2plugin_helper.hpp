#ifndef HDDL2PLUGIN_HELPER_HPP
#define HDDL2PLUGIN_HELPER_HPP

#include <condition_variable>
#include <queue>

#include <inference_engine.hpp>
#include <HddlUnite.h>

#include "common.hpp"
#include "ImageNetLabels.hpp"

namespace IE = InferenceEngine;

class HDDL2pluginHelper_t
{
public:
    using PostprocPtr_t = std::function<void(HDDL2pluginHelper_t *, IE::Blob::Ptr, std::vector<ROI> &vecROI)>;
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

//            _executableNetwork = _ie.LoadNetwork(network, "HDDL2");
            _executableNetwork = _ie.LoadNetwork(network, "HDDL2", {});
        }

		// ---- Create infer request
		// _ptrInferRequest = _executableNetwork.CreateInferRequestPtr();
		_ptrInferRequestPool = std::make_shared<InferRequestPool_t>(
				_executableNetwork, numInferRequest);
	}
    inline void inferAsyncImgPipe(IE::InferRequest::Ptr ptrInferRequest, InferCallback_t callback,
                char* hostFd, size_t heightInput = 0, size_t widthInput = 0, const ROI &roi = ROI{})
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

    void setup(int32_t numInferRequest = 1);

    void inferAsync(IE::InferRequest::Ptr ptrInferRequest, InferCallback_t callback,
                           int remoteMemoryFd, size_t heightInput, size_t widthInput, const ROI &roi = ROI{});

    IE::Blob::Ptr getOutputBlob(IE::InferRequest::Ptr ptrInferRequest);

    IE::InferRequest::Ptr getInferRequest();

    void putInferRequest(IE::InferRequest::Ptr ptrInferRequest);

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

    static int compile(const std::string &graphName);

public:
    static IE::Blob::Ptr deQuantize(const IE::Blob::Ptr &quantBlob, float scale, uint8_t zeroPoint);

    static IE::Blob::Ptr yoloLayer_yolov2tiny(const IE::Blob::Ptr &lastBlob, int inputHeight, int inputWidth);

    static std::vector<std::string> readLabelsFromFile(const std::string &labelFileName);

public:
    class InferRequestPool_t
    {
    public:
        InferRequestPool_t() = delete;

        explicit InferRequestPool_t(IE::ExecutableNetwork &executableNetwork, int32_t numInferRequest);
        ~InferRequestPool_t();

        InferRequestPool_t(const InferRequestPool_t &) = delete;
        InferRequestPool_t &operator=(const InferRequestPool_t &) = delete;

    private:
        using Type = IE::InferRequest::Ptr;

    public:
        Type get();

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

    class OrderKeeper_t
    {
    public:
        OrderKeeper_t() = default;
        ~OrderKeeper_t() = default;

        OrderKeeper_t(const OrderKeeper_t &) = delete;
        OrderKeeper_t &operator=(const OrderKeeper_t &) = delete;

        void lock(uint64_t id);

        void unlock(uint64_t id);

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

public:
};

#endif //#ifndef HDDL2PLUGIN_HELPER_HPP
