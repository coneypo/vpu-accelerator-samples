#include <Sender.hpp>
#include <cmath>

SenderNode::SenderNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const std::vector<std::string>& unixSocket):
        hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), m_unixSocket(unixSocket), m_numOfWorkers(totalThreadNum),
        m_numOfSockets(unixSocket.size()),m_workerIdx(0u){

    if(m_numOfWorkers > 1){
        hva::hvaBatchingConfig_t batchingConfig;
        batchingConfig.batchingPolicy = hva::hvaBatchingConfig_t::BatchingWithStream;
        batchingConfig.batchSize = std::ceil(m_numOfSockets/m_numOfWorkers);
        batchingConfig.streamNum = m_numOfSockets;
        batchingConfig.threadNumPerBatch = 1;

        this->configBatch(batchingConfig);
    }
}

std::shared_ptr<hva::hvaNodeWorker_t> SenderNode::createNodeWorker() const{
    unsigned workerIdx = m_workerIdx.fetch_add(1);
    unsigned socketsPerWorker = m_numOfSockets / m_numOfWorkers;
    unsigned rem = m_numOfSockets % m_numOfWorkers;

    unsigned startIdx = workerIdx < rem ? workerIdx*(socketsPerWorker+1) : rem*(socketsPerWorker+1)+(workerIdx-rem)*socketsPerWorker;
    unsigned endIdx = workerIdx < rem ? (workerIdx+1)*(socketsPerWorker+1) : rem*(socketsPerWorker+1)+(workerIdx-rem+1)*socketsPerWorker;
    std::unordered_map<unsigned, std::string> sockets;
    for(unsigned i = startIdx; i != endIdx; ++i){
        sockets[i] = m_unixSocket[i];
        HVA_DEBUG("Assigning socket %s to stream %u", m_unixSocket[i], i);
    }
    return std::shared_ptr<hva::hvaNodeWorker_t>(new SenderNodeWorker((SenderNode*)this, sockets));
}

std::string SenderNode::getUnixSocket(unsigned index) const{
    return m_unixSocket[index];
}

SenderNodeWorker::SenderNodeWorker(hva::hvaNode_t* parentNode, const std::unordered_map<unsigned, std::string>& unixSocket)
        :hva::hvaNodeWorker_t(parentNode), m_unixSocket(unixSocket){

}

void SenderNodeWorker::process(std::size_t batchIdx){
    std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput= hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});
    
    if(vInput.size()!=0){
        HVA_DEBUG("Sender received blob with frameid %u and streamid %u", vInput[0]->frameId, vInput[0]->streamId);
        unsigned streamIdx = vInput[0]->streamId;
        InferMeta* meta = vInput[0]->get<int, InferMeta>(0)->getMeta();
        float decFps = vInput[0]->get<int, VideoMeta>(1)->getMeta()->decFps;
        float inferFps = meta->inferFps;
        for(unsigned i =0; i< meta->rois.size(); ++i){
            const auto& rois = meta->rois;
            m_sender[streamIdx]->serializeSave(rois[i].x, rois[i].y, rois[i].width, rois[i].height, rois[i].labelClassification, rois[i].pts,
                    rois[i].confidenceClassification, inferFps, decFps);
        }
        m_sender[streamIdx]->send();
        HVA_DEBUG("Sender complete blob with frameid %u and streamid %u", vInput[0]->frameId, vInput[0]->streamId);
    }
}

void SenderNodeWorker::init(){
    for(const auto& pair: m_unixSocket){
        InferMetaSender* temp = new InferMetaSender();
        HVA_DEBUG("Going to connect to %s", pair.second.c_str());
        bool ret = temp->connectServer(pair.second);
        if(ret){
            m_sender[pair.first] = std::move(temp);
        }
        else{
            HVA_ERROR("Error: Fail to connect to %s", pair.second.c_str());
        }
    }
}

void SenderNodeWorker::deinit(){
    for(const auto& pair: m_sender){
        delete(pair.second);
    }
    m_sender.clear();
}
