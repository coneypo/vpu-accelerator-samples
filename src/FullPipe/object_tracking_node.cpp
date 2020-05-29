#include "object_tracking_node.hpp"
#include <algorithm>
#include <cctype>
#include <string>

//constructed by hva framework when pipeline initialize
ObjectTrackingNode::ObjectTrackingNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, 
        std::vector<WorkloadID> vWorkloadId, int32_t reservedInt,
        std::string reservedStr, const std::string& trackingModeStr)
        : hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum)
        , m_reservedInt(reservedInt), m_reservedStr(reservedStr)
        , m_vWorkloadId(vWorkloadId), m_trackingModeStr(trackingModeStr)
{

}

//called by hva framework when pipeline initialize
std::shared_ptr<hva::hvaNodeWorker_t> ObjectTrackingNode::createNodeWorker() const {
    return std::shared_ptr<hva::hvaNodeWorker_t>(new ObjectTrackingNodeWorker((ObjectTrackingNode*)this, 
    m_vWorkloadId[m_cntNodeWorker++], m_reservedInt, m_reservedStr, m_trackingModeStr));
}

//constructed by hva framework when pipeline initialize
ObjectTrackingNodeWorker::ObjectTrackingNodeWorker(
    hva::hvaNode_t* parentNode, WorkloadID workloadId, 
    int32_t reservedInt, std::string reservedStr, const std::string& trackingModeStr)
    : hva::hvaNodeWorker_t(parentNode), m_workloadId(workloadId)
    , m_reservedInt(reservedInt), m_reservedStr(reservedStr)
    , m_trackingModeStr(trackingModeStr)
{
    vas::ot::ObjectTracker::Builder builder;
    builder.backend_type = vas::BackendType::CPU;

    std::transform(m_trackingModeStr.begin(), m_trackingModeStr.end(), m_trackingModeStr.begin(),
                    [](unsigned char c){ return std::tolower(c); });

    if (m_trackingModeStr == "short_term_imageless")
        m_tracker = builder.Build(vas::ot::TrackingType::SHORT_TERM_IMAGELESS);
    else
        m_tracker = builder.Build(vas::ot::TrackingType::ZERO_TERM_IMAGELESS);
}

//called by hva framework for each video frame
void ObjectTrackingNodeWorker::process(std::size_t batchIdx)
{
    m_vecBlobInput = hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    if(m_vecBlobInput.size() != 0)
    {
        HVA_DEBUG("object tracking node start to process\n");
        auto ptrBufInfer = m_vecBlobInput[0]->get<int, InferMeta>(0);
        auto ptrBufVideo = m_vecBlobInput[0]->get<int, VideoMeta>(1);

        VideoMeta* ptrVideoMeta = ptrBufVideo->getMeta();
        int32_t input_height = static_cast<int32_t>(ptrVideoMeta->videoHeight);
        int32_t input_width = static_cast<int32_t>(ptrVideoMeta->videoWidth);

        input_height = (input_height + 63) & (~63);
        input_width = (input_width + 63) & (~63);
        
        InferMeta* ptrInferMeta = ptrBufInfer->getMeta();
        auto num_rois = ptrInferMeta->rois.size();

        auto start = std::chrono::steady_clock::now();

        {
            //prepare input for tracking
            std::vector<vas::ot::DetectedObject> detected_objects;
            for (auto& roi : ptrInferMeta->rois)
                detected_objects.push_back({cv::Rect(roi.x, roi.y, roi.height, roi.width), roi.labelIdDetection});

            //use a dummy image
            if (m_dummy.empty() || input_height != m_dummy.rows || input_width != m_dummy.cols)
            {
                m_dummy = cv::Mat(input_height, input_width, CV_8UC3);
            }

            HVA_DEBUG("Received %u detected objects at frameid %u and streamid %u", detected_objects.size(), m_vecBlobInput[0]->frameId, m_vecBlobInput[0]->streamId);

            auto tracked_objects = m_tracker->Track(m_dummy, detected_objects);

            HVA_DEBUG("Tracked %u objects at frameid %u and streamid %u", tracked_objects.size(), m_vecBlobInput[0]->frameId, m_vecBlobInput[0]->streamId);

            // Re-create ROI information based on tracking result.
            // No information will be kept except for the rectangle coordination detection label.
            // Currently, tracking API does not provide any mapping between tracking input and output.
            // It will be fixed in the next release of tracking API.
            ptrInferMeta->rois.clear();
            for (auto& to : tracked_objects)
            {
                if (to.status != vas::ot::TrackingStatus::LOST)
                {
                    assert(to.tracking_id > 0);

                    ROI roi;
                    roi.x = to.rect.x;
                    roi.x = std::max(roi.x, 0);
                    roi.x = std::min(static_cast<int32_t>(ptrVideoMeta->videoWidth), roi.x);
                    roi.y = to.rect.y;
                    roi.y = std::max(roi.y, 0);
                    roi.y = std::min(static_cast<int32_t>(ptrVideoMeta->videoHeight), roi.y);
                    roi.width = std::min(to.rect.width + to.rect.x, static_cast<int32_t>(ptrVideoMeta->videoWidth)) - roi.x;
                    roi.width = std::max(roi.width, 0);
                    roi.height = std::min(to.rect.height + to.rect.y, static_cast<int32_t>(ptrVideoMeta->videoHeight)) - roi.y;
                    roi.height = std::max(roi.height, 0);
                    roi.trackingId = to.tracking_id;
                    roi.labelIdDetection = to.class_label;
                    roi.labelClassification = "unknown"; //"unknown";
                    roi.pts = static_cast<size_t>(m_vecBlobInput[0]->frameId);
                    ptrInferMeta->rois.push_back(roi);
                }
            }
        }

        // assert(num_rois == ptrInferMeta->rois.size()); // disabled due to in dropped frames has no roi

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        m_durationAve = (m_durationAve * m_cntFrame + duration) / (m_cntFrame + 1);
        m_fps = 1000.0f / m_durationAve;
        m_cntFrame++;

        sendOutput(m_vecBlobInput[0], 0, ms(0));
    }
}

//called by hva framework when pipeline initialize
void ObjectTrackingNodeWorker::init(){
    
}
