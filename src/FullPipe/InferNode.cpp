#include <InferNode.hpp>
#include <boost/algorithm/string.hpp>

#define TOTAL_ROIS 2
#define GUI_INTEGRATION

InferNode::InferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum,
                     WorkloadID id, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc) : 
                     hva::hvaNode_t{inPortNum, outPortNum, totalThreadNum},
                     m_id{id}, m_graphPath{graphPath}, m_mode{mode}, m_postproc(postproc)
{
}

std::shared_ptr<hva::hvaNodeWorker_t> InferNode::createNodeWorker() const
{
    return std::shared_ptr<hva::hvaNodeWorker_t>{new InferNodeWorker{const_cast<InferNode*>(this), m_id, m_graphPath, m_mode, m_postproc}};
}

InferNodeWorker::InferNodeWorker(hva::hvaNode_t* parentNode, 
                                WorkloadID id, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc):
                                hva::hvaNodeWorker_t{parentNode}, m_helperHDDL2{graphPath, id, postproc}, m_mode{mode} 
{
}

void InferNodeWorker::process(std::size_t batchIdx){
    m_vecBlobInput = hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    if(m_vecBlobInput.size() != 0){
        printf("[debug] frameId is %d, graphName is %s\n", m_vecBlobInput[0]->frameId, m_mode.c_str());
        if (m_mode == "detection")
        {
            // const auto& pbuf = vInput[0]->get<int,VideoMeta>(0)->getPtr();
            // std::this_thread::sleep_for(std::chrono::milliseconds(50));

            auto ptrVideoBuf = m_vecBlobInput[0]->get<int, VideoMeta>(0);

            uint64_t fd = *((uint64_t*)ptrVideoBuf->getPtr());

            VideoMeta* ptrVideoMeta = ptrVideoBuf->getMeta();
            int input_height = ptrVideoMeta->videoHeight;
            int input_width = ptrVideoMeta->videoWidth;

            input_height = HDDL2pluginHelper_t::alignTo<64>(input_height);
            input_width = HDDL2pluginHelper_t::alignTo<64>(input_width);

            auto start = std::chrono::steady_clock::now();

            m_helperHDDL2.update(fd, input_height, input_width);
            m_helperHDDL2.infer();
            m_helperHDDL2.postproc();

            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            printf("infer node duration is %ld, mode is %s\n", duration, m_mode.c_str());
            
            m_durationAve = (m_durationAve * m_cntFrame + duration) / (m_cntFrame + 1);
            m_fps = 1000.0f / m_durationAve;
            m_cntFrame++;

            std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
            InferMeta* ptrInferMeta = new InferMeta;
            auto &vecObjects = m_helperHDDL2._vecObjects;
            for(unsigned i =0; i < vecObjects.size(); ++i){
                ROI roi;
                roi.x = vecObjects[i].x;
                roi.y = vecObjects[i].y;
                roi.width = vecObjects[i].width;
                roi.height = vecObjects[i].height;
                roi.label = "unkown";
                roi.pts = m_vecBlobInput[0]->frameId;
                roi.confidence = 0;
                roi.indexROI = i;
                ptrInferMeta->rois.push_back(roi);
            }
            ptrInferMeta->frameId = m_vecBlobInput[0]->frameId;
            ptrInferMeta->totalROI = vecObjects.size();
            ptrInferMeta->durationDetection = m_durationAve;
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

        }
        else if (m_mode == "classification")
        {

            auto ptrBufInfer = m_vecBlobInput[0]->get<int, InferMeta>(0);
            auto ptrBufVideo = m_vecBlobInput[0]->get<int, VideoMeta>(1);


            uint64_t fd = *((uint64_t*)ptrBufVideo->getPtr());

            VideoMeta* ptrVideoMeta = ptrBufVideo->getMeta();
            int input_height = ptrVideoMeta->videoHeight;
            int input_width = ptrVideoMeta->videoWidth;

            input_height = HDDL2pluginHelper_t::alignTo<64>(input_height);
            input_width = HDDL2pluginHelper_t::alignTo<64>(input_width);

            InferMeta* ptrInferMeta = ptrBufInfer->getMeta();

            std::vector<InfoROI_t> &vecROI = m_helperHDDL2._vecROI;
            vecROI.clear();
            for (auto& roi : ptrInferMeta->rois)
            {
                InfoROI_t infoROI;
                infoROI.x = roi.x;
                infoROI.y = roi.y;
                infoROI.height = roi.height;
                infoROI.width = roi.width;
                vecROI.push_back(infoROI);
            }
            if (vecROI.size() > 0)
            {
                if (ptrInferMeta->drop)
                {

                    for (int i = 0; i < ptrInferMeta->rois.size(); i++)
                    {
                        ptrInferMeta->rois[i].label = "unknown";
                        printf("[debug] roi label is : unknown\n");
                    }  
                }
                else
                {
                    auto start = std::chrono::steady_clock::now();

                    m_helperHDDL2.update(fd, input_height, input_width, vecROI);
                    m_helperHDDL2.infer();
                    m_helperHDDL2.postproc();

                    auto end = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                    printf("infer node duration is %ld, mode is %s\n", duration, m_mode.c_str());
                    
                    m_durationAve = (m_durationAve * m_cntFrame + duration) / (m_cntFrame + 1);
                    m_fps = 1000.0f / m_durationAve;
                    m_cntFrame++;


                    auto &vecROI = m_helperHDDL2._vecROI;

                    printf("[debug] input roi size: %ld, output label size: %ld\n", ptrInferMeta->rois.size(), vecROI.size());
                    assert(std::min(ptrInferMeta->rois.size(), 10ul) == vecROI.size());

                    for (int i = 0; i < ptrInferMeta->rois.size(); i++)
                    {
                        std::vector<std::string> fields;
                        boost::split(fields, vecROI[i].label, boost::is_any_of(","));
                        ptrInferMeta->rois[i].label = fields[0];
                        ptrInferMeta->rois[i].confidence = vecROI[i].confidence;
                        printf("[debug] roi label is : %s\n", vecROI[i].label.c_str());
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
                roi.confidence = 0;
                roi.label = "unknown";
                ptrInferMeta->rois.push_back(roi);
                printf("[debug] fake roi\n");
            }
            

            ptrInferMeta->durationClassification = m_durationAve;
            
            // std::this_thread::sleep_for(std::chrono::milliseconds(100));
            sendOutput(m_vecBlobInput[0], 0, ms(0));
#ifdef GUI_INTEGRATION
            sendOutput(m_vecBlobInput[0], 1, ms(0));
#endif
            m_vecBlobInput.clear();
        }
        else{
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void InferNodeWorker::init(){
    m_helperHDDL2.setup();
}