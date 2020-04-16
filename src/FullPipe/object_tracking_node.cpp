#include "object_tracking_node.hpp"


//constructed by hva framework when pipeline initialize
ObjectTrackingNode::ObjectTrackingNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, 
        std::vector<WorkloadID> vWorkloadId, int32_t reservedInt, std::string reservedStr):
hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), 
m_reservedInt(reservedInt), m_reservedStr(reservedStr), m_vWorkloadId(vWorkloadId){

}

//called by hva framework when pipeline initialize
std::shared_ptr<hva::hvaNodeWorker_t> ObjectTrackingNode::createNodeWorker() const {
    return std::shared_ptr<hva::hvaNodeWorker_t>(new ObjectTrackingNodeWorker((ObjectTrackingNode*)this, 
    m_vWorkloadId[m_cntNodeWorker++], m_reservedInt, m_reservedStr));
}

//constructed by hva framework when pipeline initialize
ObjectTrackingNodeWorker::ObjectTrackingNodeWorker(hva::hvaNode_t* parentNode,
        WorkloadID workloadId, int32_t reservedInt, std::string reservedStr):
hva::hvaNodeWorker_t(parentNode), 
m_workloadId(workloadId), m_tracker(workloadId), m_reservedInt(reservedInt), m_reservedStr(reservedStr) {

}

//called by hva framework for each video frame
void ObjectTrackingNodeWorker::process(std::size_t batchIdx)
{
    m_vecBlobInput = hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    if(m_vecBlobInput.size() != 0)
    {
        auto ptrBufInfer = m_vecBlobInput[0]->get<int, InferMeta>(0);
        auto ptrBufVideo = m_vecBlobInput[0]->get<int, VideoMeta>(1);


        int32_t fd = *((uint64_t*)ptrBufVideo->getPtr());

        VideoMeta* ptrVideoMeta = ptrBufVideo->getMeta();
        int32_t input_height = ptrVideoMeta->videoHeight;
        int32_t input_width = ptrVideoMeta->videoWidth;

        input_height = (input_height + 63) & (~63);
        input_width = (input_width + 63) & (~63);
        
        InferMeta* ptrInferMeta = ptrBufInfer->getMeta();

        auto start = std::chrono::steady_clock::now();

        //to be replaced by real tracker
        {
            //prepare input for tracking
            std::vector<FakeOT::DetectedObject> vecDetectedObject;
            for (auto& roi : ptrInferMeta->rois)
            {
                FakeOT::DetectedObject roiTemp;
                roiTemp.left = roi.x;
                roiTemp.top = roi.y;
                roiTemp.height = roi.height;
                roiTemp.width = roi.width;
                roiTemp.class_label = roi.labelIdClassification;
                vecDetectedObject.push_back(roiTemp);
            }

            //set video buffer fd/height/width
            m_tracker.setVideoBufferProperty(fd, input_height, input_width);
            
            //run tracking algorithm
            auto vecObject = m_tracker.track(vecDetectedObject);

            //fetch tracking output
            for (int32_t i = 0; i < vecObject.size(); i++)
            {
                auto& roi = ptrInferMeta->rois[i];
                roi.trackingId = vecObject[i].tracking_id;
                roi.trackingStatus = static_cast<HvaPipeline::TrackingStatus>(vecObject[i].status);
            }

        }

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