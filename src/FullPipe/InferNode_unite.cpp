#include <tuple>

#include <boost/algorithm/string.hpp>
#include "hvaLogger.hpp"

#include "InferNode_unite.hpp"
#include "hddl2plugin_helper.hpp"


InferNode_unite::InferNode_unite(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum,
            std::vector<WorkloadID> vWID, std::string graphPath, std::string graphName, int32_t inputSizeNN, int32_t outputSize):
        hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), 
        graphName(graphName), graphPath(graphPath), inputSizeNN(inputSizeNN), outputSize(outputSize), vWID(vWID){

}

std::shared_ptr<hva::hvaNodeWorker_t> InferNode_unite::createNodeWorker() const {

    std::lock_guard<std::mutex> lock{m_mutex};
    HVA_DEBUG("cntNodeWorker is %d \n", (int)m_cntNodeWorker);
    return std::shared_ptr<hva::hvaNodeWorker_t>(new InferNodeWorker_unite((InferNode_unite*)this, 
    vWID[m_cntNodeWorker++], graphName, graphPath, inputSizeNN, outputSize));
}

InferNodeWorker_unite::InferNodeWorker_unite(hva::hvaNode_t* parentNode, 
            WorkloadID id, std::string graphName, std::string graphPath, int32_t inputSizeNN, int32_t outputSize):
hva::hvaNodeWorker_t(parentNode), m_uniteHelper(id, graphName, graphPath, inputSizeNN, outputSize), m_mode{graphName}
{
    if (m_uniteHelper.getGraphName() == "resnet" || m_uniteHelper.getGraphName() == "classification")
    {
        HVA_DEBUG("initialize ObjectSelector\n");
        m_object_selector.reset(new ObjectSelector());
    }
}

void InferNodeWorker_unite::process(std::size_t batchIdx)
{
    m_vecBlobInput = hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    if(m_vecBlobInput.size() != 0)
    {
        HVA_DEBUG("frameId is %d, graphName is %s\n", m_vecBlobInput[0]->frameId, m_uniteHelper.getGraphName().c_str());
        if (m_uniteHelper.getGraphName() == "yolotiny" || m_uniteHelper.getGraphName() == "detection")
        {
            HVA_DEBUG("Detection node received blob with frameid %u and streamid %u", m_vecBlobInput[0]->frameId, m_vecBlobInput[0]->streamId);

            auto ptrVideoBuf = m_vecBlobInput[0]->get<int, VideoMeta>(0);

            uint64_t fd = *((uint64_t*)ptrVideoBuf->getPtr());

            VideoMeta* ptrVideoMeta = ptrVideoBuf->getMeta();
            if(ptrVideoMeta->drop){
                HVA_DEBUG("Detection node dropped frameid %u and streamid %u", m_vecBlobInput[0]->frameId, m_vecBlobInput[0]->streamId);
                std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
                InferMeta *ptrInferMeta = new InferMeta;
                blob->emplace<int, InferMeta>(nullptr, 0, ptrInferMeta, [](int* payload, InferMeta* meta){
                            if(payload != nullptr){
                                delete payload;
                            }
                            delete meta;
                        });
                blob->push(m_vecBlobInput[0]->get<int, VideoMeta>(0));
                blob->frameId = m_vecBlobInput[0]->frameId;
                blob->streamId = m_vecBlobInput[0]->streamId;
                sendOutput(blob, 0, ms(0));
                m_vecBlobInput.clear();
            }
            else{
                int input_height = ptrVideoMeta->videoHeight;
                int input_width = ptrVideoMeta->videoWidth;

                input_height = HDDL2pluginHelper_t::alignTo<64>(input_height);
                input_width = HDDL2pluginHelper_t::alignTo<64>(input_width);

                auto startForFps = std::chrono::steady_clock::now();
                m_uniteHelper.update(input_width, input_height, fd);
                m_uniteHelper.callInferenceOnBlobs();
                auto end = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startForFps).count();
                m_durationAve = (m_durationAve * m_cntFrame + duration) / (m_cntFrame + 1);
                m_fps = 1000.0f / m_durationAve;
                HVA_DEBUG("%s fps is %f\n", m_mode.c_str(), m_fps);
                m_cntFrame++;
                std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
                InferMeta* ptrInferMeta = new InferMeta;
                const std::vector<DetectedObject_t>& vecObjects = m_uniteHelper.getVecObjects();
                for(unsigned i = 0; i < vecObjects.size(); ++i)
                {
                    ROI roi;
                    roi.x = vecObjects[i].x;
                    roi.y = vecObjects[i].y;
                    roi.width = vecObjects[i].width;
                    roi.height = vecObjects[i].height;
                    roi.confidenceDetection = vecObjects[i].confidence;
                    roi.labelClassification = "unknown";
                    roi.pts = m_vecBlobInput[0]->frameId;
                    roi.confidenceClassification = 0;
                    ptrInferMeta->rois.push_back(roi);
                }

                HVA_DEBUG("detection frame id is %d, roi num is %ld\n", m_vecBlobInput[0]->frameId, ptrInferMeta->rois.size());

                ptrInferMeta->frameId = m_vecBlobInput[0]->frameId;
                ptrInferMeta->totalROI = vecObjects.size();
                ptrInferMeta->inferFps = m_fps;
                blob->emplace<int, InferMeta>(nullptr, 0, ptrInferMeta, [](int* payload, InferMeta* meta){
                            if(payload != nullptr){
                                delete payload;
                            }
                            delete meta;
                        });
                blob->push(m_vecBlobInput[0]->get<int, VideoMeta>(0));
                blob->frameId = m_vecBlobInput[0]->frameId;
                blob->streamId = m_vecBlobInput[0]->streamId;
                sendOutput(blob, 0, ms(0));
                m_vecBlobInput.clear();
                HVA_DEBUG("Detection node completed sending output with frameid %u and streamid %u", blob->frameId, blob->streamId);
            }
        }
        else if (m_uniteHelper.getGraphName() == "resnet" || m_uniteHelper.getGraphName() == "classification")
        {
            HVA_DEBUG("Classification node received blob with frameid %u and streamid %u", m_vecBlobInput[0]->frameId, m_vecBlobInput[0]->streamId);
            auto ptrBufInfer = m_vecBlobInput[0]->get<int, InferMeta>(0);
            auto ptrBufVideo = m_vecBlobInput[0]->get<int, VideoMeta>(1);

            uint64_t fd = *((int32_t*)ptrBufVideo->getPtr());

            VideoMeta* ptrVideoMeta = ptrBufVideo->getMeta();
            int input_height = ptrVideoMeta->videoHeight;
            int input_width = ptrVideoMeta->videoWidth;

            input_height = HDDL2pluginHelper_t::alignTo<64>(input_height);
            input_width = HDDL2pluginHelper_t::alignTo<64>(input_width);

            InferMeta* ptrInferMeta = ptrBufInfer->getMeta();
            auto num_rois = ptrInferMeta->rois.size();

            if (num_rois > 0ul)
            {
                {
                    Objects input_objects;
                    for (int32_t i = 0 ; i < num_rois; i++)
                    {
                        auto& roi = ptrInferMeta->rois[i];
                        input_objects.push_back({i, cv::Rect(roi.x, roi.y, roi.width, roi.height),
                                                roi.trackingId, -1, "", 0.0});
                    }

                    // Select new and tracked objects.
                    Objects new_objs, tracked;
                    std::tie(new_objs, tracked) = m_object_selector->preprocess(input_objects);

                    std::vector<ROI> input_vecROI;
                    for (auto& o : new_objs)
                    {
                        ROI roi = ptrInferMeta->rois[o.oid];
                        input_vecROI.push_back(roi);
                    }

                    if (input_vecROI.size() > 0ul)
                    {
                        m_uniteHelper.update(input_width, input_height, fd, input_vecROI);
                        m_uniteHelper.callInferenceOnBlobs();
                        auto& vecLabel = m_uniteHelper.getVecLabel();
                        auto& vecConfidence = m_uniteHelper.getVecConfidence();
                        auto& vecLabelId = m_uniteHelper.getVecIdx();
                        
                        assert(std::min(new_objs.size(), MAX_ROI_UNITE) == vecLabel.size());

                        for (size_t i = 0 ; i < input_vecROI.size() ; i++)
                        {
                            std::vector<std::string> fields;
                            boost::split(fields, vecLabel[i], boost::is_any_of(","));
                            new_objs[i].class_id = vecLabelId[i];
                            new_objs[i].class_label = fields[0];
                            new_objs[i].confidence_classification = static_cast<double>(vecConfidence[i]);
                        }
                    }

                    // final output objects
                    auto final_objects = m_object_selector->postprocess(new_objs, tracked);
                    for (auto& o : final_objects)
                    {
                        ptrInferMeta->rois[o.oid].labelClassification = o.class_label;
                        ptrInferMeta->rois[o.oid].labelIdClassification = o.class_id;
                        ptrInferMeta->rois[o.oid].confidenceClassification = o.confidence_classification;
                    }
                    for(const auto& o: new_objs){
                        ptrInferMeta->rois[o.oid].trackingStatus = HvaPipeline::TrackingStatus::NEW; //to be used by jpeg
                    }
                }       
            }
            else
            {
                ROI roi;
                roi.x = 0;
                roi.y = 0;
                roi.height = 0;
                roi.width = 0;
                roi.pts = m_vecBlobInput[0]->frameId;
                roi.confidenceClassification = 0;
                roi.labelClassification = "unknown";
                ptrInferMeta->rois.push_back(roi);
                HVA_DEBUG("fake roi\n");
            }

            sendOutput(m_vecBlobInput[0], 0, ms(0));
            sendOutput(m_vecBlobInput[0], 1, ms(0));
#ifdef VALIDATION_DUMP
            sendOutput(m_vecBlobInput[0], 2, ms(0));
#endif //#ifdef VALIDATION_DUMP
            m_vecBlobInput.clear();
            HVA_DEBUG("Classification node completed sending output with frameid %u and streamid %u", m_vecBlobInput[0]->frameId, m_vecBlobInput[0]->streamId);

        }
        else{
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void InferNodeWorker_unite::init(){
    m_uniteHelper.setup();
}
