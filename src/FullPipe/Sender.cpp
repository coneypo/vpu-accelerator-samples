#include <Sender.hpp>

SenderNode::SenderNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const std::string& unixSocket):
        hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), m_unixSocket(unixSocket){

}

std::shared_ptr<hva::hvaNodeWorker_t> SenderNode::createNodeWorker() const{
    return std::shared_ptr<hva::hvaNodeWorker_t>(new SenderNodeWorker((SenderNode*)this));
}

std::string SenderNode::getUnixSocket() const{
    return m_unixSocket;
}

SenderNodeWorker::SenderNodeWorker(hva::hvaNode_t* parentNode):hva::hvaNodeWorker_t(parentNode){

}

void SenderNodeWorker::process(std::size_t batchIdx){
    std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput= hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    
    if(vInput.size()!=0){
        InferMeta* meta = vInput[0]->get<int, InferMeta>(0)->getMeta();
        std::cout<<"Sender Received blob with idx "<<vInput[0]->frameId<<std::endl;
        float decFps = vInput[0]->get<int, VideoMeta>(1)->getMeta()->decFps;
        float inferFps = meta->inferFps;
        for(unsigned i =0; i< meta->rois.size(); ++i){
            const auto& rois = meta->rois;
            m_sender.serializeSave(rois[i].x, rois[i].y, rois[i].width, rois[i].height, rois[i].label, rois[i].pts,
                    rois[i].confidence, inferFps, decFps);
        }
        m_sender.send();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void SenderNodeWorker::init(){
    m_sender.connectServer(((SenderNode*)m_parentNode)->getUnixSocket());
}
