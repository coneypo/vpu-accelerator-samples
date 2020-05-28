#include <ImgInferNode.hpp>
#include <boost/algorithm/string.hpp>
#include <chrono>

#define TOTAL_ROIS 2
#define INFER_ASYNC

ImgInferNode::ImgInferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum,
            std::vector<WorkloadID> vWID, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc) 
            : hva::hvaNode_t{inPortNum, outPortNum, totalThreadNum},
            m_vWID{vWID}, m_graphPath{graphPath}, m_mode{mode}, m_postproc(postproc)
{
}

std::shared_ptr<hva::hvaNodeWorker_t> ImgInferNode::createNodeWorker() const
{
    std::lock_guard<std::mutex> lock{m_mutex};
    printf("[debug] cntNodeWorker is %d \n", (int)m_cntNodeWorker);
    return std::shared_ptr<hva::hvaNodeWorker_t>{
        new ImgInferNodeWorker{const_cast<ImgInferNode *>(this), m_vWID[m_cntNodeWorker++], m_graphPath, m_mode, m_postproc}};
}

ImgInferNodeWorker::ImgInferNodeWorker(hva::hvaNode_t *parentNode,
                                 WorkloadID id, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc) 
                                 : hva::hvaNodeWorker_t{parentNode}, m_helperHDDL2{graphPath, id, postproc}, m_mode{mode}
{
}

void ImgInferNodeWorker::process(std::size_t batchIdx)
{
    auto vecBlobInput = hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t>{0});

    if (vecBlobInput.size() != 0)
    {
//        auto start = std::chrono::steady_clock::now();
//        printf("[debug] frameId is %d, graphName is %s\n", vecBlobInput[0]->frameId, m_mode.c_str());
        if (m_mode == "detection")
        {
            auto ptrVideoBuf = vecBlobInput[0]->get<char, ImageMeta>(0);

            char* fdHost = ptrVideoBuf->getPtr();

            ImageMeta *ptrVideoMeta = ptrVideoBuf->getMeta();

            if(ptrVideoMeta->drop){
                std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
                InferMeta *ptrInferMeta = new InferMeta;
                blob->emplace<int, InferMeta>(nullptr, 0, ptrInferMeta, [](int* payload, InferMeta* meta){
                            if(payload != nullptr){
                                delete payload;
                            }
                            delete meta;
                        });
                blob->push(vecBlobInput[0]->get<int, ImageMeta>(0));
                blob->frameId = vecBlobInput[0]->frameId;
                blob->streamId = vecBlobInput[0]->streamId;
                sendOutput(blob, 0, ms(0));
            }
            else{
                int input_height = ptrVideoMeta->imageHeight;
                int input_width = ptrVideoMeta->imageWidth;

                auto startForFps = std::chrono::steady_clock::now();
                auto ptrInferRequest = m_helperHDDL2.getInferRequest();



                auto callback = [=]() mutable
                {
//                    m_cntAsyncEnd++;
//                    printf("[debug] detection async end, cnt is: %d\n", (int)m_cntAsyncEnd);
//
//                    printf("[debug] detection callback start, frame id is: %d\n", vecBlobInput[0]->frameId);
                    auto ptrOutputBlob = m_helperHDDL2.getOutputBlob(ptrInferRequest);

                    auto startPosProc = std::chrono::steady_clock::now();
                    std::vector<ROI> vecObjects;
                    m_helperHDDL2.postproc(ptrOutputBlob, vecObjects);
                    auto endPosProc = std::chrono::steady_clock::now();
                    auto durationPosProc = std::chrono::duration_cast<std::chrono::milliseconds>(endPosProc - startPosProc).count();


                    printf("postproc duration is %ld, mode is %s\n", durationPosProc, m_mode.c_str());

                    m_helperHDDL2.putInferRequest(ptrInferRequest); //can this be called before postproc?

                    // end = std::chrono::steady_clock::now();
                    // duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                    // printf("infer node duration is %ld, mode is %s\n", duration, m_mode.c_str());
                    
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endPosProc - startForFps).count();
                    m_durationAve = (m_durationAve * m_cntFrame + duration) / (m_cntFrame + 1);
                    m_fps = 1000.0f / m_durationAve;
                    m_cntFrame++;

//                    printf("postproc duration is %ld, mode is %s\n", duration, m_mode.c_str());

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
//                        roi.labelClassification = "unknown";
                        roi.labelClassification = "label" + std::to_string(vecObjects[i].labelIdDetection);
                        roi.pts = vecBlobInput[0]->frameId;
//                        roi.confidenceClassification = 0;
                        roi.confidenceClassification = vecObjects[i].confidenceDetection;
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
                    blob->push(vecBlobInput[0]->get<char, ImageMeta>(0));
                    blob->frameId = vecBlobInput[0]->frameId;
                    blob->streamId = vecBlobInput[0]->streamId;
                    sendOutput(blob, 0, ms(0));
                    vecBlobInput.clear();
//                    printf("[debug] detection callback end, frame id is: %d\n", vecBlobInput[0]->frameId);
                    return;
                };


                auto start = std::chrono::steady_clock::now();

//                m_helperHDDL2.inferAsync(ptrInferRequest, callback,
//                		fdHost, input_height, input_width);

                m_helperHDDL2.inferAsyncImgPipe(ptrInferRequest, callback,
                		fdHost, input_height, input_width);

                // for sync model
//                callback();

//                m_cntAsyncStart++;
//                printf("[debug] detection async start, cnt is: %d\n", (int32_t)m_cntAsyncStart);

                auto end = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                printf("pure async inference duration is %ld, mode is %s\n", duration, m_mode.c_str());
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

//        auto end = std::chrono::steady_clock::now();
//        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
//        printf("[debug] infer node duration is %ld, mode is %s\n", duration, m_mode.c_str());
    }
}

void ImgInferNodeWorker::init()
{
//    m_helperHDDL2.setup();
    m_helperHDDL2.setupImgPipe();
}
