#include <validationDumpNode.hpp>

ValidationDumpNode::ValidationDumpNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, std::string networkName)
        :m_networkName(networkName), hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum){
    
}

std::shared_ptr<hva::hvaNodeWorker_t> ValidationDumpNode::createNodeWorker() const{
    return std::shared_ptr<hva::hvaNodeWorker_t>(new ValidationDumpNodeWorker((ValidationDumpNode*)this, m_networkName));
}

ValidationDumpNodeWorker::ValidationDumpNodeWorker(hva::hvaNode_t* parentNode, std::string networkName)
        :m_networkName(networkName), hva::hvaNodeWorker_t(parentNode){

}

void ValidationDumpNodeWorker::init(){
    m_of.open("ValidationDump.csv");
    if(!m_of.is_open()){
        std::cout<<"Error opening ValidationDump.csv file"<<std::endl;
    }
}

void ValidationDumpNodeWorker::process(std::size_t batchIdx){
    std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput= hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    
    if(vInput.size()!=0){
#ifdef VALIDATION_DUMP
        InferMeta* inferMeta = vInput[0]->get<int, InferMeta>(0)->getMeta();
        VideoMeta* videoMeta = vInput[0]->get<int, VideoMeta>(1)->getMeta();
        videoMeta->frameEnd = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now().time_since_epoch());
        for(const auto& item: inferMeta->rois){
            if(m_of.is_open()){
                m_of << videoMeta->frameStart.count() << ", " << videoMeta->frameEnd.count() <<", 0, 0,";
                m_of << vInput[0]->streamId << ", " << item.pts << ", ";
                m_of << item.x << ", " << item.y << ", "<< item.width << ", "<< item.height << ", ";
                m_of << item.trackingId << ", " << item.labelIdDetection << ", " << item.confidenceDetection<<", ";
                m_of << "1, " << m_networkName << ", "<< item.labelIdClassification <<", "<< item.confidenceClassification <<std::endl;
            }
        }
#endif
    }
}

void ValidationDumpNodeWorker::deinit(){
    m_of.close();
}

ValidationDumpNodeWorker::~ValidationDumpNodeWorker(){
    if(m_of.is_open()){
        m_of.close();
    }
}