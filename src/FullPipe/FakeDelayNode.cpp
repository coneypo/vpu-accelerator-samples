#include <FakeDelayNode.hpp>

#define TOTAL_ROIS 2
#define GUI_INTEGRATION

FakeDelayNode::FakeDelayNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, std::string mode):
        hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), m_mode(mode){

}

std::shared_ptr<hva::hvaNodeWorker_t> FakeDelayNode::createNodeWorker() const{
    return std::shared_ptr<hva::hvaNodeWorker_t>(new FakeDelayNodeWorker((FakeDelayNode*)this, m_mode));
}

FakeDelayNodeWorker::FakeDelayNodeWorker(hva::hvaNode_t* parentNode, std::string mode):hva::hvaNodeWorker_t(parentNode), m_mode(mode){

}

void FakeDelayNodeWorker::process(std::size_t batchIdx){
    std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput= hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    if(vInput.size() != 0){
        if(m_mode == "detection"){
            // const auto& pbuf = vInput[0]->get<int,VideoMeta>(0)->getPtr();
            std::cout<<"Det received blob with streamid "<<vInput[0]->streamId<<" and frameid "<<vInput[0]->frameId<<std::endl;
            // std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
            InferMeta* meta = new InferMeta;
            for(unsigned i =0; i < TOTAL_ROIS; ++i){
                ROI roi;
                roi.x = 900+(i*100);
                roi.y = 500+(i*100);
                roi.width = 200;
                roi.height = 200;
                roi.labelClassification = "unknown";
                roi.pts = vInput[0]->frameId;
                roi.confidenceClassification = 0.9+i/100.0;
                // roi.indexROI = i;
                meta->rois.push_back(roi);
            }
            meta->frameId = vInput[0]->frameId;
            meta->totalROI = TOTAL_ROIS;
            meta->inferFps = 20.20;
            blob->streamId = vInput[0]->streamId;
            blob->frameId = vInput[0]->frameId;
            blob->emplace<int, InferMeta>(nullptr, 0, meta, [](int* payload, InferMeta* meta){
                        if(payload != nullptr){
                            delete payload;
                        }
                        delete meta;
                    });
            blob->push(vInput[0]->get<int, VideoMeta>(0));
            sendOutput(blob, 0, ms(0));
            std::cout<<"Det Sent blob with streamid "<<vInput[0]->streamId<<" and frameid "<<vInput[0]->frameId<<std::endl;
        }
        else if(m_mode == "classification"){
            std::cout<<"Cls received blob with streamid "<<vInput[0]->streamId<<" and frameid "<<vInput[0]->frameId<<std::endl;
            // std::this_thread::sleep_for(std::chrono::milliseconds(100));
            sendOutput(vInput[0], 0, ms(0));
#ifdef GUI_INTEGRATION
            sendOutput(vInput[0], 1, ms(0));
#endif
            std::cout<<"Cls Sent blob with streamid "<<vInput[0]->streamId<<" and frameid "<<vInput[0]->frameId<<std::endl;
        }
        else{
            // std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void FakeDelayNodeWorker::init(){

}