#include <FrameControlNode.hpp>

FrameControlNode::FrameControlNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, unsigned dropEveryXFrame):
        hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), m_dropEveryXFrame(dropEveryXFrame){

}

std::shared_ptr<hva::hvaNodeWorker_t> FrameControlNode::createNodeWorker() const{
    return std::shared_ptr<hva::hvaNodeWorker_t>(new FrameControlNodeWorker((FrameControlNode*)this, m_dropEveryXFrame));
}

FrameControlNodeWorker::FrameControlNodeWorker(hva::hvaNode_t* parentNode, unsigned dropEveryXFrame):hva::hvaNodeWorker_t(parentNode), 
        m_dropEveryXFrame(dropEveryXFrame), m_cnt(0u){

}

void FrameControlNodeWorker::process(std::size_t batchIdx){
    std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput= hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    if(vInput.size() != 0){
        if(m_dropEveryXFrame != 0 && m_cnt !=0 && m_cnt%m_dropEveryXFrame ==0){
            ++m_cnt;
            return;
        }
        else{
            ++m_cnt;
            sendOutput(vInput[0], 0);
        }
    }
}

void FrameControlNodeWorker::init(){
    if(m_dropEveryXFrame == 1){
        m_dropEveryXFrame = 2;
    }
    if(m_dropEveryXFrame > 8){
        m_dropEveryXFrame = 8;
    }
}