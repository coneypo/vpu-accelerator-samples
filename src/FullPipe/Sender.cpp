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

struct Packet{
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    std::string label;
    std::size_t pts;
    double confidence;
};


void SenderNodeWorker::process(std::size_t batchIdx){
    std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput= hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    Packet* tosend = vInput[0]->get<Packet>(0)->getPtr();
    m_sender.serializeSave(tosend->x, tosend->y, tosend->width, tosend->height, tosend->label, tosend->pts, tosend->confidence);
    m_sender.send();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void SenderNodeWorker::init(){
    m_sender.connectServer(((SenderNode*)m_parentNode)->getUnixSocket());
}

void SenderNodeWorker::process(std::size_t batchIdx){

}
