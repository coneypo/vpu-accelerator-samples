#include <ImgInferNode.hpp>
#include <boost/algorithm/string.hpp>
#include <chrono>

#define TOTAL_ROIS 2
#define INFER_ASYNC
//#define VALIDATION_DUMP

ImgInferNode::ImgInferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum,
            std::vector<WorkloadID> vWID, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc,
            int32_t numInferRequest, float thresholdDetection)
            : hva::hvaNode_t{inPortNum, outPortNum, totalThreadNum},
            m_vWID{vWID}, m_graphPath{graphPath}, m_mode{mode}, m_postproc(postproc),
			m_numInferRequest{numInferRequest}, m_thresholdDetection{thresholdDetection}
{
}

std::shared_ptr<hva::hvaNodeWorker_t> ImgInferNode::createNodeWorker() const
{
    std::lock_guard<std::mutex> lock{m_mutex};
//    printf("[debug] cntNodeWorker is %d \n", (int)m_cntNodeWorker);
    return std::shared_ptr<hva::hvaNodeWorker_t>{
        new ImgInferNodeWorker{const_cast<ImgInferNode *>(this), m_vWID[m_cntNodeWorker++], m_graphPath, m_mode, m_postproc,
            m_numInferRequest, m_thresholdDetection}};
}

ImgInferNodeWorker::ImgInferNodeWorker(hva::hvaNode_t *parentNode,
                                 WorkloadID id, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc,
	                                int32_t numInferRequest, float thresholdDetection)
                                 : hva::hvaNodeWorker_t{parentNode}, m_helperHDDL2{graphPath, id, postproc, thresholdDetection}, m_mode{mode},
	                                m_numInferRequest{numInferRequest}, m_thresholdDetection{thresholdDetection}
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
                    auto ptrOutputBlob = m_helperHDDL2.getOutputBlob(ptrInferRequest);

                    auto startPosProc = std::chrono::steady_clock::now();
                    std::vector<ROI> vecObjects;
                    m_helperHDDL2.postproc(ptrOutputBlob, vecObjects);
                    auto endPosProc = std::chrono::steady_clock::now();
                    auto durationPosProc = std::chrono::duration_cast<std::chrono::milliseconds>(endPosProc - startPosProc).count();


                    printf("postproc duration is %ld, mode is %s\n", durationPosProc, m_mode.c_str());

                    m_helperHDDL2.putInferRequest(ptrInferRequest); //can this be called before postproc?

                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endPosProc - startForFps).count();
                    m_durationAve = (m_durationAve * m_cntFrame + duration) / (m_cntFrame + 1);
                    m_fps = 1000.0f / m_durationAve;
                    m_cntFrame++;

                    printf("Inference FPS is %f, mode is %s\n", m_fps, m_mode.c_str());

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
                        roi.labelClassification = "label" + std::to_string(vecObjects[i].labelIdDetection);
                        roi.pts = vecBlobInput[0]->frameId;
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
                    return;
                };

                m_helperHDDL2.inferAsyncImgPipe(ptrInferRequest, callback,
                		fdHost, input_height, input_width);
                // for sync model
//                callback();
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void ImgInferNodeWorker::init()
{
    m_helperHDDL2.setupImgPipe(m_numInferRequest);
}
