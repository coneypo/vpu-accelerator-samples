#include "InferNode.hpp"

#include <chrono>

#include <boost/algorithm/string.hpp>
#include "hvaLogger.hpp"

InferNode::InferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum,
            std::vector<WorkloadID> vWID, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc, 
            int32_t numInferRequest, float thresholdDetection) 
            : hva::hvaNode_t{inPortNum, outPortNum, totalThreadNum},
            m_vWID{vWID}, m_graphPath{graphPath}, m_mode{mode}, m_postproc(postproc),
            m_numInferRequest{numInferRequest}, m_thresholdDetection{thresholdDetection}
{
}

std::shared_ptr<hva::hvaNodeWorker_t> InferNode::createNodeWorker() const
{
    std::lock_guard<std::mutex> lock{m_mutex};
    HVA_DEBUG("cntNodeWorker is %d \n", (int)m_cntNodeWorker);
    return std::shared_ptr<hva::hvaNodeWorker_t>{
        new InferNodeWorker{const_cast<InferNode *>(this), m_vWID[m_cntNodeWorker++], m_graphPath, m_mode, m_postproc, 
                            m_numInferRequest, m_thresholdDetection}};
}

InferNodeWorker::InferNodeWorker(hva::hvaNode_t *parentNode,
                                WorkloadID id, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc, 
                                int32_t numInferRequest, float thresholdDetection) 
                                : hva::hvaNodeWorker_t{parentNode}, m_helperHDDL2{graphPath, id, postproc, thresholdDetection}, m_mode{mode},
                                m_numInferRequest{numInferRequest}, m_thresholdDetection{thresholdDetection}
{
    HVA_DEBUG("m_numInferRequest is %d, m_thresholdDetection is %f", m_numInferRequest, m_thresholdDetection);
    if (m_mode == "classification")
    {
        HVA_DEBUG("initialize ObjectSelector\n");
        m_object_selector.reset(new ObjectSelector());
    }
}

void InferNodeWorker::process(std::size_t batchIdx)
{
    auto vecBlobInput = hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t>{0});

    if (vecBlobInput.size() != 0)
    {
        HVA_DEBUG("frameId is %d, graphName is %s\n", vecBlobInput[0]->frameId, m_mode.c_str());
        if (m_mode == "detection")
        {
            auto ptrVideoBuf = vecBlobInput[0]->get<int, VideoMeta>(0);

            int32_t fd = *((int32_t *)ptrVideoBuf->getPtr());

            VideoMeta *ptrVideoMeta = ptrVideoBuf->getMeta();

            if(ptrVideoMeta->drop){
                std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
                InferMeta *ptrInferMeta = new InferMeta;
                ptrInferMeta->frameId = vecBlobInput[0]->frameId;
                ptrInferMeta->totalROI = 0;
                ptrInferMeta->inferFps = m_ips;
                blob->emplace<int, InferMeta>(nullptr, 0, ptrInferMeta, [](int* payload, InferMeta* meta){
                            if(payload != nullptr){
                                delete payload;
                            }
                            delete meta;
                        });
                blob->push(vecBlobInput[0]->get<int, VideoMeta>(0));
                blob->frameId = vecBlobInput[0]->frameId;
                blob->streamId = vecBlobInput[0]->streamId;
                HVA_DEBUG("Detection completed sending blob with frameid %u and streamid %u", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId);
                sendOutput(blob, 0, ms(0));
                HVA_DEBUG("Detection completed sent blob with frameid %u and streamid %u", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId);
            }
            else{
                int input_height = ptrVideoMeta->videoHeight;
                int input_width = ptrVideoMeta->videoWidth;

                input_height = HDDL2pluginHelper_t::alignTo<64>(input_height);
                input_width = HDDL2pluginHelper_t::alignTo<64>(input_width);

                static auto startForIps = std::chrono::steady_clock::now();
                auto startForLatency = std::chrono::steady_clock::now();
                auto ptrInferRequest = m_helperHDDL2.getInferRequest();

                auto callback = [=]() mutable
                {
                    m_cntAsyncEnd++;
                    HVA_DEBUG("detection async end, frame id is: %d, stream id is: %d, cnt is: %d\n", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId, (int)m_cntAsyncEnd);

                    HVA_DEBUG("detection callback start, frame id is: %d, stream id is: %d\n", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId);
                    auto ptrOutputBlob = m_helperHDDL2.getOutputBlob(ptrInferRequest);

                    auto start = std::chrono::steady_clock::now();
                    std::vector<ROI> vecObjects;
                    m_helperHDDL2.postproc(ptrOutputBlob, vecObjects);
                    auto end = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                    HVA_DEBUG("postproc duration is %ld, mode is %s\n", duration, m_mode.c_str());

                    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startForLatency).count();
                    m_durationAve = (m_durationAve * m_cntFrame + duration) / (m_cntFrame + 1);
                    m_cntFrame++;
                    m_cntInfer++;
                    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startForIps).count();
                    m_ips = m_cntInfer * 1000.0f / duration;

                    std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
                    InferMeta *ptrInferMeta = new InferMeta;
                    for (unsigned i = 0; i < vecObjects.size(); ++i)
                    {
                        ROI roi;
                        roi.x = vecObjects[i].x;
                        roi.y = vecObjects[i].y;
                        roi.width = vecObjects[i].width;
                        roi.height = vecObjects[i].height;
                        roi.labelClassification = "unknown";
                        roi.pts = vecBlobInput[0]->frameId;
                        roi.confidenceClassification = 0;
                        roi.confidenceDetection = vecObjects[i].confidenceDetection;
                        roi.labelIdDetection = vecObjects[i].labelIdDetection;
                        ptrInferMeta->rois.push_back(roi);
                    }
                    ptrInferMeta->frameId = vecBlobInput[0]->frameId;
                    ptrInferMeta->totalROI = vecObjects.size();
                    ptrInferMeta->inferFps = m_ips;
                    blob->emplace<int, InferMeta>(nullptr, 0, ptrInferMeta, [](int* payload, InferMeta* meta){
                                if(payload != nullptr){
                                    delete payload;
                                }
                                delete meta;
                            });
                    blob->push(vecBlobInput[0]->get<int, VideoMeta>(0));
                    blob->frameId = vecBlobInput[0]->frameId;
                    blob->streamId = vecBlobInput[0]->streamId;
                    HVA_DEBUG("Detection completed sending blob with frameid %u and streamid %u", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId);
                    sendOutput(blob, 0, ms(0));
                    HVA_DEBUG("Detection completed sent blob with frameid %u and streamid %u", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId);
                    vecBlobInput.clear();
                    HVA_DEBUG("detection callback end, frame id is: %d, stream id is: %d\n", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId);
                    m_helperHDDL2.putInferRequest(ptrInferRequest);
                    return;
                };
                auto start = std::chrono::steady_clock::now();

                m_helperHDDL2.inferAsync(ptrInferRequest, callback,
                    fd, input_height, input_width);

                m_cntAsyncStart++;
                HVA_DEBUG("detection async start, frame id is: %d, stream id is: %d, cnt is: %d\n", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId, (int32_t)m_cntAsyncStart);

                auto end = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                HVA_DEBUG("pure async inference duration is %ld, mode is %s\n", duration, m_mode.c_str());
                HVA_DEBUG("Detection completed sending blob with frameid %u and streamid %u", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId);
            }
        }
        else if (m_mode == "classification")
        {
            auto ptrBufInfer = vecBlobInput[0]->get<int, InferMeta>(0);
            auto ptrBufVideo = vecBlobInput[0]->get<int, VideoMeta>(1);

            int32_t fd = *((int32_t *)ptrBufVideo->getPtr());

            VideoMeta *ptrVideoMeta = ptrBufVideo->getMeta();
            int input_height = ptrVideoMeta->videoHeight;
            int input_width = ptrVideoMeta->videoWidth;

            input_height = HDDL2pluginHelper_t::alignTo<64>(input_height);
            input_width = HDDL2pluginHelper_t::alignTo<64>(input_width);

            InferMeta *ptrInferMeta = ptrBufInfer->getMeta();

            std::vector<ROI>& vecROI = ptrInferMeta->rois;

            if (vecROI.size() > 0)
            {
                {
                    //preprocess for object selection
                    std::shared_ptr<Objects> ptr_input_objects = std::make_shared<Objects>();
                    if (nullptr == ptr_input_objects)
                    {
                        throw std::runtime_error("make_shared fail");
                    }
                    for (int32_t i = 0; i < vecROI.size(); i++)
                    {
                        auto& roi = ptrInferMeta->rois[i];
                        ptr_input_objects->push_back({i, cv::Rect(roi.x, roi.y, roi.width, roi.height),
                                                roi.trackingId, -1, "", 0.0});
                    }
                    
                    // Select new and tracked objects.
                    std::shared_ptr<Objects> ptr_new_objs = std::make_shared<Objects>();
                    if (nullptr == ptr_new_objs)
                    {
                        throw std::runtime_error("make_shared fail");
                    }
                    std::shared_ptr<Objects> ptr_tracked = std::make_shared<Objects>();
                    if (nullptr == ptr_tracked)
                    {
                        throw std::runtime_error("make_shared fail");
                    }
                    std::tie(*ptr_new_objs, *ptr_tracked) = m_object_selector->preprocess(*ptr_input_objects);

                    std::shared_ptr<std::vector<ROI>> ptr_input_vecROI = std::make_shared<std::vector<ROI>>();
                    if (nullptr == ptr_input_vecROI)
                    {
                        throw std::runtime_error("make_shared fail");
                    }

                    for (auto& o : *ptr_new_objs)
                    {
                        ROI roi = ptrInferMeta->rois[o.oid];
                        ptr_input_vecROI->push_back(roi);
                    }

                    if(ptr_input_vecROI->size() > 0)
                    {
                        static auto startForIps = std::chrono::steady_clock::now();
                        auto startForLatency = std::chrono::steady_clock::now();
                        
                        std::shared_ptr<std::mutex> ptrMutex = std::make_shared<std::mutex>();
                        if (nullptr == ptrMutex)
                        {
                            throw std::runtime_error("make_shared fail");
                        }
                        std::shared_ptr<int32_t> ptrCntCallback{new int32_t};
                        *ptrCntCallback = 0;

                        for (int cntROI = 0; cntROI < ptr_input_vecROI->size(); cntROI++)
                        {
                            auto ptrInferRequest = m_helperHDDL2.getInferRequest();
                            auto callback = [=]() mutable
                            {
                                m_cntAsyncEnd++;
                                HVA_DEBUG("classification async end, frame id is: %d, stream id is: %d, cnt is: %d\n", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId, (int)m_cntAsyncEnd);

                                HVA_DEBUG("classification callback start, frame id is: %d, stream id is: %d\n", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId);
                                auto ptrOutputBlob = m_helperHDDL2.getOutputBlob(ptrInferRequest);

                                auto start = std::chrono::steady_clock::now();
                                
                                std::vector<ROI>& vecROI = *ptr_input_vecROI;
                                std::vector<ROI> vecROITemp;
                                m_helperHDDL2.postproc(ptrOutputBlob, vecROITemp);


                                auto end = std::chrono::steady_clock::now();
                                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                                HVA_DEBUG("postproc duration is %ld, mode is %s\n", duration, m_mode.c_str());
                                
                                HVA_DEBUG("input roi size: %ld, output label size: %ld\n", ptrInferMeta->rois.size(), vecROITemp.size());
                                assert(1 == vecROITemp.size());

                                std::vector<std::string> fields;
                                boost::split(fields, vecROITemp[0].labelClassification, boost::is_any_of(","));
                                vecROI[cntROI].labelClassification = fields[0];
                                vecROI[cntROI].confidenceClassification = vecROITemp[0].confidenceClassification;
                                vecROI[cntROI].labelIdClassification = vecROITemp[0].labelIdClassification;
                                HVA_DEBUG("roi label is : %s\n", vecROITemp[0].labelClassification.c_str());

                                {
                                    std::lock_guard<std::mutex> lock{*ptrMutex}; 
                                    auto& cntCallback= *ptrCntCallback;                           
                                    (cntCallback)++;

                                    if(cntCallback == vecROI.size())
                                    {

                                        end = std::chrono::steady_clock::now();
                                        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startForLatency).count();
                                        m_durationAve = (m_durationAve * m_cntFrame + duration) / (m_cntFrame + 1);
                                        m_cntFrame++;
                                        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startForIps).count();
                                        m_cntInfer += vecROI.size();
                                        m_ips = m_cntInfer * 1000.0f / duration;

                                        ptrInferMeta->inferFps += m_ips;

                                        HVA_DEBUG("classification whole duration is %ld, mode is %s\n", duration, m_mode.c_str());

                                        // fetch output object info
                                        for (size_t i = 0 ; i < vecROI.size() ; i++)
                                        {
                                            std::vector<std::string> fields;
                                            boost::split(fields, vecROI[i].labelClassification, boost::is_any_of(","));
                                            (*ptr_new_objs)[i].class_id = vecROI[i].labelIdClassification;
                                            (*ptr_new_objs)[i].class_label = fields[0];
                                            (*ptr_new_objs)[i].confidence_classification = static_cast<double>(vecROI[i].confidenceClassification);
                                        }
                                        // final output objects

                                        HVA_DEBUG("object selection postprocess start, frame id is: %d\n", ptrInferMeta->frameId);
                                        for(const auto& o : *ptr_new_objs)
                                        {
                                            HVA_DEBUG("new object, track id is: %ld\n", o.tracking_id);
                                        }
                                        for(const auto& o : *ptr_tracked)
                                        {
                                            HVA_DEBUG("tracked object, track id is: %ld\n", o.tracking_id);
                                        }
                                        m_orderKeeper.lock(ptrInferMeta->frameId);
                                        auto final_objects = m_object_selector->postprocess(*ptr_new_objs, *ptr_tracked);
                                        m_orderKeeper.unlock(ptrInferMeta->frameId);
                                        for (auto& o : final_objects)
                                        {
                                            HVA_DEBUG("final objects oid is %d, label is %s\n", o.oid, o.class_label.c_str());
                                            ptrInferMeta->rois[o.oid].labelClassification = o.class_label;
                                            ptrInferMeta->rois[o.oid].labelIdClassification = o.class_id;
                                            ptrInferMeta->rois[o.oid].confidenceClassification = o.confidence_classification;
                                        }
                                        for(const auto& o: *ptr_new_objs){
                                            ptrInferMeta->rois[o.oid].trackingStatus = HvaPipeline::TrackingStatus::NEW; //to be used by jpeg
                                        }

                                        sendOutput(vecBlobInput[0], 0, ms(0));
                                        sendOutput(vecBlobInput[0], 1, ms(0));

#ifdef VALIDATION_DUMP
                                        sendOutput(vecBlobInput[0], 2, ms(0));
#endif //#ifdef VALIDATION_DUMP
                                    }

                                }
                                vecBlobInput.clear();
                                HVA_DEBUG("classification callback end, frame id is %d, stream id is %d\n", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId);
                                m_helperHDDL2.putInferRequest(ptrInferRequest); //can this be called before postproc?
                                return;
                            };

                            auto start = std::chrono::steady_clock::now();
                            m_helperHDDL2.inferAsync(ptrInferRequest, callback,
                                fd, input_height, input_width, vecROI[cntROI]);
                            m_cntAsyncStart++;
                            HVA_DEBUG("classification async start, frame id is: %d, stream id is %d, cnt is: %d\n", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId, (int32_t)m_cntAsyncStart);

                            auto end = std::chrono::steady_clock::now();
                            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                            HVA_DEBUG("pure async inference duration is %ld, mode is %s\n", duration, m_mode.c_str());
                        }
                    }
                    else
                    {
                        m_orderKeeper.lock(ptrInferMeta->frameId);
                        auto final_objects = m_object_selector->postprocess(*ptr_new_objs, *ptr_tracked);
                        m_orderKeeper.unlock(ptrInferMeta->frameId);

                        for (auto& o : final_objects)
                        {
                            ptrInferMeta->rois[o.oid].labelClassification = o.class_label;
                            ptrInferMeta->rois[o.oid].labelIdClassification = o.class_id;
                            ptrInferMeta->rois[o.oid].confidenceClassification = o.confidence_classification;
                        }

                        ptrInferMeta->inferFps += m_ips;
                        sendOutput(vecBlobInput[0], 0, ms(0));
                        sendOutput(vecBlobInput[0], 1, ms(0));

        #ifdef VALIDATION_DUMP
                        sendOutput(vecBlobInput[0], 2, ms(0));
        #endif //#ifdef VALIDATION_DUMP
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
                roi.pts = vecBlobInput[0]->frameId;
                roi.confidenceClassification = 0;
                roi.labelClassification = "unknown";
                ptrInferMeta->rois.push_back(roi);
                HVA_DEBUG("fake roi\n");
                            
                m_orderKeeper.bypass(ptrInferMeta->frameId);

                ptrInferMeta->inferFps += m_ips;
                sendOutput(vecBlobInput[0], 0, ms(0));
                sendOutput(vecBlobInput[0], 1, ms(0));
#ifdef VALIDATION_DUMP
                sendOutput(vecBlobInput[0], 2, ms(0));
#endif //#ifdef VALIDATION_DUMP
                
            }
            HVA_DEBUG("classification loop end, frame id is %d, stream id is %d\n", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId);

        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void InferNodeWorker::init()
{
    HVA_DEBUG("infer request number is %d", m_numInferRequest);
    if (m_mode == "detection" || m_mode == "classification")
    {
        m_helperHDDL2.setup(m_numInferRequest);
    }
    else
    {
        m_helperHDDL2.setup();
    }
}