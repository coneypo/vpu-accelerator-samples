#include <InferNode.hpp>
#include <boost/algorithm/string.hpp>
#include <chrono>

#define TOTAL_ROIS 2
#define INFER_ASYNC
#define VALIDATION_DUMP

InferNode::InferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum,
            std::vector<WorkloadID> vWID, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc) 
            : hva::hvaNode_t{inPortNum, outPortNum, totalThreadNum},
            m_vWID{vWID}, m_graphPath{graphPath}, m_mode{mode}, m_postproc(postproc)
{
}

std::shared_ptr<hva::hvaNodeWorker_t> InferNode::createNodeWorker() const
{
    std::lock_guard<std::mutex> lock{m_mutex};
    printf("[debug] cntNodeWorker is %d \n", (int)m_cntNodeWorker);
    return std::shared_ptr<hva::hvaNodeWorker_t>{
        new InferNodeWorker{const_cast<InferNode *>(this), m_vWID[m_cntNodeWorker++], m_graphPath, m_mode, m_postproc}};
}

InferNodeWorker::InferNodeWorker(hva::hvaNode_t *parentNode,
                                 WorkloadID id, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc) 
                                 : hva::hvaNodeWorker_t{parentNode}, m_helperHDDL2{graphPath, id, postproc}, m_mode{mode}
{
    if (m_mode == "classification")
    {
        printf("initialize ObjectSelector\n");
        m_object_selector.reset(new ObjectSelector());
    }
}

void InferNodeWorker::process(std::size_t batchIdx)
{
    auto vecBlobInput = hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t>{0});

    if (vecBlobInput.size() != 0)
    {
        auto start = std::chrono::steady_clock::now();
        printf("[debug] frameId is %d, graphName is %s\n", vecBlobInput[0]->frameId, m_mode.c_str());
        if (m_mode == "detection")
        {
            // const auto& pbuf = vInput[0]->get<int,VideoMeta>(0)->getPtr();
            // std::this_thread::sleep_for(std::chrono::milliseconds(50));

            auto ptrVideoBuf = vecBlobInput[0]->get<int, VideoMeta>(0);

            int32_t fd = *((int32_t *)ptrVideoBuf->getPtr());

            VideoMeta *ptrVideoMeta = ptrVideoBuf->getMeta();

            if(ptrVideoMeta->drop){
                std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
                InferMeta *ptrInferMeta = new InferMeta;
                blob->emplace<int, InferMeta>(nullptr, 0, ptrInferMeta, [](int* payload, InferMeta* meta){
                            if(payload != nullptr){
                                delete payload;
                            }
                            delete meta;
                        });
                blob->push(vecBlobInput[0]->get<int, VideoMeta>(0));
                blob->frameId = vecBlobInput[0]->frameId;
                blob->streamId = vecBlobInput[0]->streamId;
                sendOutput(blob, 0, ms(0));
                HVA_DEBUG("Detection completed sending blob with frameid %u and streamid %u", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId);
            }
            else{
                int input_height = ptrVideoMeta->videoHeight;
                int input_width = ptrVideoMeta->videoWidth;

                input_height = HDDL2pluginHelper_t::alignTo<64>(input_height);
                input_width = HDDL2pluginHelper_t::alignTo<64>(input_width);

    #ifndef INFER_ASYNC
                auto start = std::chrono::steady_clock::now();
                auto startForFps = start;
                m_helperHDDL2.update(fd, input_height, input_width);
                auto end = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                printf("update duration is %ld, mode is %s\n", duration, m_mode.c_str());

                start = std::chrono::steady_clock::now();
                auto ptrOutputBlob = m_helperHDDL2.infer();

                end = std::chrono::steady_clock::now();
                duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                printf("pure sync inference duration is %ld, mode is %s\n", duration, m_mode.c_str());

                start = std::chrono::steady_clock::now();
                std::vector<ROI> vecObjects;
                m_helperHDDL2.postproc(ptrOutputBlob, vecObjects);
                end = std::chrono::steady_clock::now();
                duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                printf("postproc duration is %ld, mode is %s\n", duration, m_mode.c_str());

                // end = std::chrono::steady_clock::now();
                // duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                // printf("infer node duration is %ld, mode is %s\n", duration, m_mode.c_str());
                
                duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startForFps).count();
                m_durationAve = (m_durationAve * m_cntFrame + duration) / (m_cntFrame + 1);
                m_fps = 1000.0f / m_durationAve;
                m_cntFrame++;

    #if 1
                if(m_cntFrame == 1)
                {
                    FILE* fp = fopen("dump-output-sync.bin", "wb");
                    fwrite(ptrOutputBlob->buffer(), 1, ptrOutputBlob->byteSize(), fp);
                    fclose(fp);
                }

    #endif

                std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
                InferMeta *ptrInferMeta = new InferMeta;
                // auto &vecObjects = m_helperHDDL2._vecObjects;
                for (unsigned i = 0; i < vecObjects.size(); ++i)
                {
                    ROI roi;
                    roi.x = vecObjects[i].x;
                    roi.y = vecObjects[i].y;
                    roi.width = vecObjects[i].width;
                    roi.height = vecObjects[i].height;
                    roi.labelClassification = "unkown";
                    roi.pts = vecBlobInput[0]->frameId;
                    roi.confidenceClassification = 0;
                    // roi.indexROI = i;
                    ptrInferMeta->rois.push_back(roi);
                }
                ptrInferMeta->frameId = vecBlobInput[0]->frameId;
                ptrInferMeta->totalROI = vecObjects.size();
                ptrInferMeta->durationDetection = m_durationAve;
                ptrInferMeta->inferFps = m_fps;
                blob->emplace<int, InferMeta>(nullptr, 0, ptrInferMeta, [](int* payload, InferMeta* meta){
                            if(payload != nullptr){
                                delete payload;
                            }
                            delete meta;
                        });
                blob->push(vecBlobInput[0]->get<int, VideoMeta>(0));
                blob->frameId = vecBlobInput[0]->frameId;
                blob->streamId = vecBlobInput[0]->streamId;
                sendOutput(blob, 0, ms(0));
                vecBlobInput.clear();
    #else //#ifndef INFER_ASYNC

                auto startForFps = std::chrono::steady_clock::now();
                auto ptrInferRequest = m_helperHDDL2.getInferRequest();



                auto callback = [=]() mutable
                {
                    m_cntAsyncEnd++;
                    printf("[debug] detection async end, cnt is: %d\n", (int)m_cntAsyncEnd);

                    printf("[debug] detection callback start, frame id is: %d\n", vecBlobInput[0]->frameId);
                    auto ptrOutputBlob = m_helperHDDL2.getOutputBlob(ptrInferRequest);

                    auto start = std::chrono::steady_clock::now();
                    std::vector<ROI> vecObjects;
                    m_helperHDDL2.postproc(ptrOutputBlob, vecObjects);
                    auto end = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                    printf("postproc duration is %ld, mode is %s\n", duration, m_mode.c_str());

                    m_helperHDDL2.putInferRequest(ptrInferRequest); //can this be called before postproc?

                    // end = std::chrono::steady_clock::now();
                    // duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                    // printf("infer node duration is %ld, mode is %s\n", duration, m_mode.c_str());
                    
                    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startForFps).count();
                    m_durationAve = (m_durationAve * m_cntFrame + duration) / (m_cntFrame + 1);
                    m_fps = 1000.0f / m_durationAve;
                    m_cntFrame++;
    #if 0
                    if(m_cntFrame == 1)
                    {
                        FILE* fp = fopen("dump-output-async.bin", "wb");
                        fwrite(ptrOutputBlob->buffer(), 1, ptrOutputBlob->byteSize(), fp);
                        fclose(fp);
                    }
    #endif
                    std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
                    InferMeta *ptrInferMeta = new InferMeta;
                    // auto &vecObjects = m_helperHDDL2._vecObjects;
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
                        // roi.indexROI = i;
                        ptrInferMeta->rois.push_back(roi);
                    }
                    ptrInferMeta->frameId = vecBlobInput[0]->frameId;
                    ptrInferMeta->totalROI = vecObjects.size();
                    ptrInferMeta->durationDetection = m_durationAve;
                    ptrInferMeta->inferFps = m_fps;
                    blob->emplace<int, InferMeta>(nullptr, 0, ptrInferMeta, [](int* payload, InferMeta* meta){
                                if(payload != nullptr){
                                    delete payload;
                                }
                                delete meta;
                            });
                    blob->push(vecBlobInput[0]->get<int, VideoMeta>(0));
                    blob->frameId = vecBlobInput[0]->frameId;
                    blob->streamId = vecBlobInput[0]->streamId;
                    sendOutput(blob, 0, ms(0));
                    vecBlobInput.clear();
                    printf("[debug] detection callback end, frame id is: %d\n", vecBlobInput[0]->frameId);
                    return;
                };
                auto start = std::chrono::steady_clock::now();

                m_helperHDDL2.inferAsync(ptrInferRequest, callback,
                    fd, input_height, input_width);

                m_cntAsyncStart++;
                printf("[debug] detection async start, cnt is: %d\n", (int32_t)m_cntAsyncStart);

                auto end = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                printf("pure async inference duration is %ld, mode is %s\n", duration, m_mode.c_str());
                HVA_DEBUG("Detection completed sending blob with frameid %u and streamid %u", vecBlobInput[0]->frameId, vecBlobInput[0]->streamId);
            }

#endif //#ifndef INFER_ASYNC

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
            // for (auto &roi : ptrInferMeta->rois)
            // {
            //     ROI roiTemp;
            //     roiTemp.x = roi.x;
            //     roiTemp.y = roi.y;
            //     roiTemp.height = roi.height;
            //     roiTemp.width = roi.width;
            //     vecROI.push_back(roiTemp);
            // }
            if (vecROI.size() > 0)
            {
#if 0 //disable drop flag
                if (ptrInferMeta->drop)
                {
                    for (int i = 0; i < ptrInferMeta->rois.size(); i++)
                    {
                        ptrInferMeta->rois[i].labelClassification = "unknown";
                        printf("[debug] roi label is : unknown\n");
                    }
                    ptrInferMeta->durationClassification = m_durationAve;

                    m_orderKeeper.bypass(ptrInferMeta->frameId);
                    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    sendOutput(vecBlobInput[0], 0, ms(0));
                    sendOutput(vecBlobInput[0], 1, ms(0));
                }
                else
#endif 
                {

#ifndef INFER_ASYNC
                    auto start = std::chrono::steady_clock::now();
                    auto startForFps = start;
                    m_helperHDDL2.update(fd, input_height, input_width, vecROI);
                    auto end = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                    printf("update duration is %ld, mode is %s\n", duration, m_mode.c_str());

                    start = std::chrono::steady_clock::now();
                    auto ptrOutputBlob = m_helperHDDL2.infer();
                    end = std::chrono::steady_clock::now();
                    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                    printf("pure sync inference duration is %ld, mode is %s\n", duration, m_mode.c_str());

                    start = std::chrono::steady_clock::now();
#ifdef SINGLE_ROI_POSTPROC
                    std::vector<ROI>  vecROITemp;
                    m_helperHDDL2.postproc(ptrOutputBlob, vecROITemp);
#else
                    m_helperHDDL2.postproc(ptrOutputBlob, vecROI);
#endif
                    end = std::chrono::steady_clock::now();
                    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                    printf("postproc duration is %ld, mode is %s\n", duration, m_mode.c_str());
                    
                    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startForFps).count();
                    m_durationAve = (m_durationAve * m_cntFrame + duration) / (m_cntFrame + 1);
                    m_fps = 1000.0f / m_durationAve;
                    m_cntFrame++;

                    printf("postproc duration is %ld, mode is %s\n", duration, m_mode.c_str());

                    // auto &vecROI = m_helperHDDL2._vecROI;

                    printf("[debug] input roi size: %ld, output label size: %ld\n", ptrInferMeta->rois.size(), vecROI.size());
                    assert(std::min(ptrInferMeta->rois.size(), 10ul) == vecROI.size());
#ifdef SINGLE_ROI_POSTPROC

                    for (int i = 0; i < ptrInferMeta->rois.size(); i++)
                    {
                        std::vector<std::string> fields;
                        boost::split(fields, vecROITemp[0].labelClassification, boost::is_any_of(","));
                        ptrInferMeta->rois[i].labelClassification = fields[0];
                        ptrInferMeta->rois[i].confidenceClassification = vecROITemp[0].confidenceClassification;
                        printf("[debug] roi label is : %s\n", vecROITemp[0].labelClassification.c_str());
                    }
#else
                    for (int i = 0; i < ptrInferMeta->rois.size(); i++)
                    {
                        std::vector<std::string> fields;
                        boost::split(fields, vecROI[i].labelClassification, boost::is_any_of(","));
                        ptrInferMeta->rois[i].labelClassification = fields[0];
                        ptrInferMeta->rois[i].confidenceClassification = vecROI[i].confidenceClassification;
                        printf("[debug] roi label is : %s\n", vecROI[i].labelClassification.c_str());
                    }
#endif //#ifdef SINGLE_ROI_POSTPROC
                    ptrInferMeta->durationClassification = m_durationAve;
            
                    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    sendOutput(vecBlobInput[0], 0, ms(0));
                    sendOutput(vecBlobInput[0], 1, ms(0));

#else //#ifndef INFER_ASYNC
                    //preprocess for object selection
                    std::shared_ptr<Objects> ptr_input_objects = std::make_shared<Objects>();
                    for (int32_t i = 0; i < vecROI.size(); i++)
                    {
                        auto& roi = ptrInferMeta->rois[i];
                        ptr_input_objects->push_back({i, cv::Rect(roi.x, roi.y, roi.width, roi.height),
                                                roi.trackingId, -1, "", 0.0});
                    }


                    m_orderKeeper.lock(ptrInferMeta->frameId);
                    
                    // Select new and tracked objects.
                    std::shared_ptr<Objects> ptr_new_objs = std::make_shared<Objects>();
                    std::shared_ptr<Objects> ptr_tracked = std::make_shared<Objects>();
                    std::tie(*ptr_new_objs, *ptr_tracked) = m_object_selector->preprocess(*ptr_input_objects);

                    std::shared_ptr<std::vector<ROI>> ptr_input_vecROI = std::make_shared<std::vector<ROI>>();
                    // input_vecROI.clear();
                    for (auto& o : *ptr_new_objs)
                    {
                        ROI roi = ptrInferMeta->rois[o.oid];
                        ptr_input_vecROI->push_back(roi);
                    }

                    if(ptr_input_vecROI->size() > 0)
                    {
                        auto startForFps = std::chrono::steady_clock::now();

                        //todo, fix me
                        
                        std::shared_ptr<std::mutex> ptrMutex = std::make_shared<std::mutex>();
                        std::shared_ptr<int32_t> ptrCntCallback{new int32_t};
                        *ptrCntCallback = 0;

                        for (int cntROI = 0; cntROI < ptr_input_vecROI->size(); cntROI++)
                        {
                            auto ptrInferRequest = m_helperHDDL2.getInferRequest();
                            auto callback = [=]() mutable
                            {
                                m_cntAsyncEnd++;
                                printf("[debug] classification async end, frame id is: %d, cnt is: %d\n", vecBlobInput[0]->frameId, (int)m_cntAsyncEnd);

                                printf("[debug] classification callback start, frame id is: %d\n", vecBlobInput[0]->frameId);
                                auto ptrOutputBlob = m_helperHDDL2.getOutputBlob(ptrInferRequest);

                                auto start = std::chrono::steady_clock::now();
                                
                                std::vector<ROI>& vecROI = *ptr_input_vecROI;
                                std::vector<ROI> vecROITemp;
                                m_helperHDDL2.postproc(ptrOutputBlob, vecROITemp);

                                m_helperHDDL2.putInferRequest(ptrInferRequest); //can this be called before postproc?

                                auto end = std::chrono::steady_clock::now();
                                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                                printf("postproc duration is %ld, mode is %s\n", duration, m_mode.c_str());
                                
                                printf("[debug] input roi size: %ld, output label size: %ld\n", ptrInferMeta->rois.size(), vecROITemp.size());
                                assert(1 == vecROITemp.size());

                                // for (int i = 0; i < ptrInferMeta->rois.size(); i++)
                                {
                                    std::vector<std::string> fields;
                                    boost::split(fields, vecROITemp[0].labelClassification, boost::is_any_of(","));
                                    vecROI[cntROI].labelClassification = fields[0];
                                    vecROI[cntROI].confidenceClassification = vecROITemp[0].confidenceClassification;
                                    printf("[debug] roi label is : %s\n", vecROITemp[0].labelClassification.c_str());
                                }

                                {
                                    std::lock_guard<std::mutex> lock{*ptrMutex}; 
                                    auto& cntCallback= *ptrCntCallback;                           
                                    (cntCallback)++;

                                    if(cntCallback == vecROI.size())
                                    {

                                        end = std::chrono::steady_clock::now();
                                        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startForFps).count();
                                        m_durationAve = (m_durationAve * m_cntFrame + duration) / (m_cntFrame + 1);
                                        m_fps = 1000.0f / m_durationAve;
                                        m_cntFrame++;

                                        printf("classification whole duration is %ld, mode is %s\n", duration, m_mode.c_str());
                                        ptrInferMeta->durationClassification = m_durationAve;

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

                                        printf("[debug] postprocess start, frame id is: %d\n", ptrInferMeta->frameId);
                                        for(const auto& o : *ptr_new_objs)
                                        {
                                            printf("[debug] new object, track id is: %ld\n", o.tracking_id);
                                        }
                                        for(const auto& o : *ptr_tracked)
                                        {
                                            printf("[debug] tracked object, track id is: %ld\n", o.tracking_id);
                                        }
                                        auto final_objects = m_object_selector->postprocess(*ptr_new_objs, *ptr_tracked);
                                        m_orderKeeper.unlock(ptrInferMeta->frameId);
                                        for (auto& o : final_objects)
                                        {
                                            ptrInferMeta->rois[o.oid].labelClassification = o.class_label;
                                            ptrInferMeta->rois[o.oid].labelIdClassification = o.class_id;
                                            ptrInferMeta->rois[o.oid].confidenceClassification = o.confidence_classification;
                                        }

                                        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                        sendOutput(vecBlobInput[0], 0, ms(0));
                #ifdef GUI_INTEGRATION
                                        sendOutput(vecBlobInput[0], 1, ms(0));
                #endif
                #ifdef VALIDATION_DUMP
                                        sendOutput(vecBlobInput[0], 2, ms(0));
                #endif //#ifdef VALIDATION_DUMP
                                    }

                                }
                                vecBlobInput.clear();
                                printf("[debug] classification callback end, frame id is %d\n", vecBlobInput[0]->frameId);
                                return;
                            };

                            //todo fix me
                            auto start = std::chrono::steady_clock::now();
                            m_helperHDDL2.inferAsync(ptrInferRequest, callback,
                                fd, input_height, input_width, vecROI[cntROI]);
                            m_cntAsyncStart++;
                            printf("[debug] classification async start, frame id is: %d, cnt is: %d\n", vecBlobInput[0]->frameId, (int32_t)m_cntAsyncStart);

                            auto end = std::chrono::steady_clock::now();
                            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                            printf("pure async inference duration is %ld, mode is %s\n", duration, m_mode.c_str());
                        }
                    }
                    else
                    {                        
                        ptrInferMeta->durationClassification = m_durationAve;
                    
                        auto final_objects = m_object_selector->postprocess(*ptr_new_objs, *ptr_tracked);
                        m_orderKeeper.unlock(ptrInferMeta->frameId);

                        for (auto& o : final_objects)
                        {
                            ptrInferMeta->rois[o.oid].labelClassification = o.class_label;
                            ptrInferMeta->rois[o.oid].labelIdClassification = o.class_id;
                            ptrInferMeta->rois[o.oid].confidenceClassification = o.confidence_classification;
                        }

                        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        sendOutput(vecBlobInput[0], 0, ms(0));
        #ifdef GUI_INTEGRATION
                        sendOutput(vecBlobInput[0], 1, ms(0));
        #endif

        #ifdef VALIDATION_DUMP
                        sendOutput(vecBlobInput[0], 2, ms(0));
        #endif //#ifdef VALIDATION_DUMP
                    }
#endif //#ifndef INFER_ASYNC
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
                printf("[debug] fake roi\n");
                
                ptrInferMeta->durationClassification = m_durationAve;
            
                m_orderKeeper.bypass(ptrInferMeta->frameId);

                // std::this_thread::sleep_for(std::chrono::milliseconds(100));
                sendOutput(vecBlobInput[0], 0, ms(0));
                sendOutput(vecBlobInput[0], 1, ms(0));
#ifdef VALIDATION_DUMP
                sendOutput(vecBlobInput[0], 2, ms(0));
#endif //#ifdef VALIDATION_DUMP
                
            }
            printf("[debug] classification loop end, frame id is %d\n", vecBlobInput[0]->frameId);

        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        printf("[debug] infer node duration is %ld, mode is %s\n", duration, m_mode.c_str());
    }
}

void InferNodeWorker::init()
{
    m_helperHDDL2.setup();
}