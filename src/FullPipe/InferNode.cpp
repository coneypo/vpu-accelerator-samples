#include <InferNode.hpp>
#include <boost/algorithm/string.hpp>
#include <chrono>

#define TOTAL_ROIS 2
#define GUI_INTEGRATION
#define INFER_ASYNC

InferNode::InferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum,
                     WorkloadID id, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc) : hva::hvaNode_t{inPortNum, outPortNum, totalThreadNum},
                                                                                                                            m_id{id}, m_graphPath{graphPath}, m_mode{mode}, m_postproc(postproc)
{
}

std::shared_ptr<hva::hvaNodeWorker_t> InferNode::createNodeWorker() const
{
    return std::shared_ptr<hva::hvaNodeWorker_t>{new InferNodeWorker{const_cast<InferNode *>(this), m_id, m_graphPath, m_mode, m_postproc}};
}

InferNodeWorker::InferNodeWorker(hva::hvaNode_t *parentNode,
                                 WorkloadID id, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc) : hva::hvaNodeWorker_t{parentNode}, m_helperHDDL2{graphPath, id, postproc}, m_mode{mode}
{
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
            auto callback = [=](){
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
            };
            auto start = std::chrono::steady_clock::now();
            
            m_helperHDDL2.inferAsync(ptrInferRequest, callback,
                fd, input_height, input_width);

            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            printf("pure sync inference duration is %ld, mode is %s\n", duration, m_mode.c_str());


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
                if (ptrInferMeta->drop)
                {

                    for (int i = 0; i < ptrInferMeta->rois.size(); i++)
                    {
                        ptrInferMeta->rois[i].labelClassification = "unknown";
                        printf("[debug] roi label is : unknown\n");
                    }
                    ptrInferMeta->durationClassification = m_durationAve;
            
                    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    sendOutput(vecBlobInput[0], 0, ms(0));
#ifdef GUI_INTEGRATION
                    sendOutput(vecBlobInput[0], 1, ms(0));
#endif
                }
                else
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
                    // std::vector<ROI>  vecROI;
                    m_helperHDDL2.postproc(ptrOutputBlob, vecROI);

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

                    for (int i = 0; i < ptrInferMeta->rois.size(); i++)
                    {
                        std::vector<std::string> fields;
                        boost::split(fields, vecROI[i].labelClassification, boost::is_any_of(","));
                        ptrInferMeta->rois[i].labelClassification = fields[0];
                        ptrInferMeta->rois[i].confidenceClassification = vecROI[i].confidenceClassification;
                        printf("[debug] roi label is : %s\n", vecROI[i].labelClassification.c_str());
                    }
                    ptrInferMeta->durationClassification = m_durationAve;
            
                    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    sendOutput(vecBlobInput[0], 0, ms(0));
#ifdef GUI_INTEGRATION
                    sendOutput(vecBlobInput[0], 1, ms(0));
#endif
#else //#ifndef INFER_ASYNC

                    //todo, fix me
                    std::shared_ptr<std::mutex> ptrMutex = std::make_shared<std::mutex>();
                    int32_t cntCallback = 0;
                    for (int cntROI = 0; cntROI < vecROI.size(); cntROI++)
                    {

                    }

                    auto startForFps = std::chrono::steady_clock::now();
                    auto ptrInferRequest = m_helperHDDL2.getInferRequest();
                    auto callback = [=](){

                        auto ptrOutputBlob = m_helperHDDL2.getOutputBlob(ptrInferRequest);

                        auto start = std::chrono::steady_clock::now();
                        
                        std::vector<ROI>  vecROITemp;
                        m_helperHDDL2.postproc(ptrOutputBlob, vecROITemp);

                        m_helperHDDL2.putInferRequest(ptrInferRequest); //can this be called before postproc?

                        auto end = std::chrono::steady_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                        printf("postproc duration is %ld, mode is %s\n", duration, m_mode.c_str());
                        
                        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startForFps).count();
                        m_durationAve = (m_durationAve * m_cntFrame + duration) / (m_cntFrame + 1);
                        m_fps = 1000.0f / m_durationAve;
                        m_cntFrame++;

                        printf("postproc duration is %ld, mode is %s\n", duration, m_mode.c_str());

                        printf("[debug] input roi size: %ld, output label size: %ld\n", ptrInferMeta->rois.size(), vecROITemp.size());
                        assert(1 == vecROITemp.size());

                        for (int i = 0; i < ptrInferMeta->rois.size(); i++)
                        {
                            std::vector<std::string> fields;
                            boost::split(fields, vecROITemp[0].labelClassification, boost::is_any_of(","));
                            ptrInferMeta->rois[i].labelClassification = fields[0];
                            ptrInferMeta->rois[i].confidenceClassification = vecROITemp[0].confidenceClassification;
                            printf("[debug] roi label is : %s\n", vecROITemp[0].labelClassification.c_str());
                        }
                        ptrInferMeta->durationClassification = m_durationAve;
                
                        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        sendOutput(vecBlobInput[0], 0, ms(0));
#ifdef GUI_INTEGRATION
                        sendOutput(vecBlobInput[0], 1, ms(0));
#endif
                        return;
                    };

                    //todo fix me
                    auto start = std::chrono::steady_clock::now();
                    m_helperHDDL2.inferAsync(ptrInferRequest, callback,
                        fd, input_height, input_width, vecROI[0]);

                    auto end = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                    printf("pure sync inference duration is %ld, mode is %s\n", duration, m_mode.c_str());

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
            
                // std::this_thread::sleep_for(std::chrono::milliseconds(100));
                sendOutput(vecBlobInput[0], 0, ms(0));
#ifdef GUI_INTEGRATION
                sendOutput(vecBlobInput[0], 1, ms(0));
#endif
                
            }

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