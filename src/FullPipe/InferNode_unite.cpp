#include <InferNode_unite.hpp>
#include <boost/algorithm/string.hpp>
#include "hddl2plugin_helper.hpp"

#define TOTAL_ROIS 2
#define GUI_INTEGRATION

InferNode_unite::InferNode_unite(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum,
            WorkloadID id, std::string graphPath, std::string graphName, int32_t inputSizeNN, int32_t outputSize):
        hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), 
        graphName(graphName), graphPath(graphPath), inputSizeNN(inputSizeNN), outputSize(outputSize), id(id){

}

std::shared_ptr<hva::hvaNodeWorker_t> InferNode_unite::createNodeWorker() const{
    return std::shared_ptr<hva::hvaNodeWorker_t>(new InferNodeWorker_unite((InferNode_unite*)this, 
    id, graphName, graphPath, inputSizeNN, outputSize));
}

InferNodeWorker_unite::InferNodeWorker_unite(hva::hvaNode_t* parentNode, 
            WorkloadID id, std::string graphName, std::string graphPath, int32_t inputSizeNN, int32_t outputSize):
hva::hvaNodeWorker_t(parentNode), m_uniteHelper(id, graphName, graphPath, inputSizeNN, outputSize), m_mode{graphName}{

}

void InferNodeWorker_unite::process(std::size_t batchIdx)
{
    m_vecBlobInput = hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    if(m_vecBlobInput.size() != 0)
    {
        auto start = std::chrono::steady_clock::now();
        printf("[debug] frameId is %d, graphName is %s\n", m_vecBlobInput[0]->frameId, m_uniteHelper.graphName.c_str());
        if (m_uniteHelper.graphName == "yolotiny" || m_uniteHelper.graphName == "detection")
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
            auto startForFps = start;
            m_uniteHelper.update(input_width, input_height, fd);
            m_uniteHelper.callInferenceOnBlobs();
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            printf("update&infer duration is %ld, mode is %s\n", duration, m_mode.c_str());

            duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startForFps).count();
            m_durationAve = (m_durationAve * m_cntFrame + duration) / (m_cntFrame + 1);
            m_fps = 1000.0f / m_durationAve;
            printf("[debug] %s fps is %f\n", m_mode.c_str(), m_fps);
            m_cntFrame++;
            std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
            InferMeta* ptrInferMeta = new InferMeta;
            auto& vecObjects = m_uniteHelper._vecOjects;
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

            printf("[debug] detection frame id is %d, roi num is %ld\n", m_vecBlobInput[0]->frameId, ptrInferMeta->rois.size());

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
        else if (m_uniteHelper.graphName == "resnet" || m_uniteHelper.graphName == "classification")
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

            std::vector<InfoROI_t>& vecROI = m_uniteHelper._vecROI;
            vecROI.clear();

            printf("[debug] classification frame id is %d, roi num is %ld\n", m_vecBlobInput[0]->frameId, ptrInferMeta->rois.size());

            for (auto& roi : ptrInferMeta->rois)
            {
                InfoROI_t infoROI;
                infoROI.x = roi.x;
                infoROI.y = roi.y;
                infoROI.height = roi.height;
                infoROI.width = roi.width;
                vecROI.push_back(infoROI);
            }
            if (vecROI.size() > 0ul)
            {
                m_uniteHelper.update(input_width, input_height, fd, vecROI);
                m_uniteHelper.callInferenceOnBlobs();
                auto& vecLabel = m_uniteHelper._vecLabel;
                auto& vecConfidence = m_uniteHelper._vecConfidence;

                

                printf("[debug] input roi size: %ld, output label size: %ld\n", ptrInferMeta->rois.size(), vecLabel.size());
                assert(std::min(ptrInferMeta->rois.size(), 10ul) == vecLabel.size());

                for (int i = 0; i < ptrInferMeta->rois.size(); i++)
                {
                    std::vector<std::string> fields;
                    boost::split(fields, vecLabel[i], boost::is_any_of(","));
                    ptrInferMeta->rois[i].label = fields[0];
                    ptrInferMeta->rois[i].confidence = vecConfidence[i];
                    printf("[debug] roi label is : %s\n", vecLabel[i].c_str());
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
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        printf("[debug] infer node duration is %ld, mode is %s\n", duration, m_mode.c_str());
    }
}

void InferNodeWorker_unite::init(){
    m_uniteHelper.setup();
}