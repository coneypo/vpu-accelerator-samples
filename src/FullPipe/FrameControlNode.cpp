#include <FrameControlNode.hpp>
#include <unistd.h>
#include <sys/syscall.h>

FrameControlNode::FrameControlNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const Config& config):
        hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), m_dropEveryXFrame(config.dropEveryXFrame), m_dropXFrame(config.dropXFrame){

}

std::shared_ptr<hva::hvaNodeWorker_t> FrameControlNode::createNodeWorker() const{
    return std::shared_ptr<hva::hvaNodeWorker_t>(new FrameControlNodeWorker((FrameControlNode*)this, m_dropXFrame, m_dropEveryXFrame));
}

FrameControlNodeWorker::FrameControlNodeWorker(hva::hvaNode_t* parentNode, unsigned dropXFrame, unsigned dropEveryXFrame):hva::hvaNodeWorker_t(parentNode), 
        m_dropEveryXFrame(dropEveryXFrame), m_dropXFrame(dropXFrame){

}

void FrameControlNodeWorker::process(std::size_t batchIdx){
    std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput= hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});

    if(vInput.size() != 0){
        HVA_DEBUG("FRC received blob with frameid %u and streamid %u", vInput[0]->frameId, vInput[0]->streamId);
        unsigned streamIdx = vInput[0]->streamId;
        VideoMeta* pVideoMeta = vInput[0]->get<int, VideoMeta>(0)->getMeta();
        bool drop = true;

        const auto& item = m_cntMap.find(streamIdx);
        if(item == m_cntMap.end()){
            m_cntMap[streamIdx] = 0;
            drop = false;
        }
        else{
            if(m_dropXFrame == 0 || m_cntMap[streamIdx] ==0){
                // sendOutput(vInput[0], 0, ms(0));
                drop = false;
            }
            else{
                if(m_cntMap[streamIdx] > m_dropXFrame){
                    // sendOutput(vInput[0], 0, ms(0));
                    drop = false;
                }
            }
        }
        pVideoMeta->drop = drop;
        sendOutput(vInput[0], 0, ms(0));
        incCount(streamIdx);
        HVA_DEBUG("FRC sent blob with frameid %u and streamid %u", vInput[0]->frameId, vInput[0]->streamId);
    }
}

void FrameControlNodeWorker::init(){
    if(m_dropXFrame == 0){
        m_dropEveryXFrame = 1024;
    }
    else{
        if(m_dropEveryXFrame > 1024){
            m_dropEveryXFrame = 1024;
        }
        else{
            if(m_dropEveryXFrame <= 1){
                m_dropEveryXFrame = 2;
            }
        }
        
        if(m_dropXFrame >= m_dropEveryXFrame){
            m_dropXFrame = m_dropEveryXFrame - 1;
        }
    }
}

void FrameControlNodeWorker::incCount(unsigned streamIdx){
    if(++m_cntMap[streamIdx] > m_dropEveryXFrame){
        m_cntMap[streamIdx] = 1;
    }
}