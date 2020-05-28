#include <ImgFrameControlNode.hpp>
#include <unistd.h>
#include <sys/syscall.h>

ImgFrameControlNode::ImgFrameControlNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const Config& config):
        hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum), m_dropEveryXFrame(0), m_dropXFrame(0){

}

std::shared_ptr<hva::hvaNodeWorker_t> ImgFrameControlNode::createNodeWorker() const{
    return std::shared_ptr<hva::hvaNodeWorker_t>(new ImgFrameControlNodeWorker((ImgFrameControlNode*)this, m_dropXFrame, m_dropEveryXFrame));
}

ImgFrameControlNodeWorker::ImgFrameControlNodeWorker(hva::hvaNode_t* parentNode, unsigned dropXFrame, unsigned dropEveryXFrame):hva::hvaNodeWorker_t(parentNode), 
        m_dropEveryXFrame(dropEveryXFrame), m_dropXFrame(dropXFrame), m_cnt(0u){

}

void ImgFrameControlNodeWorker::process(std::size_t batchIdx){
    std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput= hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});

    if(vInput.size() != 0){
        HVA_DEBUG("FRC received blob with frameid %u and streamid %u", vInput[0]->frameId, vInput[0]->streamId);
        ImageMeta* pVideoMeta = vInput[0]->get<char, ImageMeta>(0)->getMeta();
        bool drop = true;
        if(m_dropXFrame == 0 || m_cnt ==0){
            // sendOutput(vInput[0], 0, ms(0));
            drop = false;
        }
        else{
            if(m_cnt > m_dropXFrame){
                // sendOutput(vInput[0], 0, ms(0));
                drop = false;
            }
        }
        pVideoMeta->drop = drop;
        sendOutput(vInput[0], 0, ms(0));
        incCount();
        HVA_DEBUG("FRC sent blob with frameid %u and streamid %u", vInput[0]->frameId, vInput[0]->streamId);
    }
}

void ImgFrameControlNodeWorker::init(){
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

void ImgFrameControlNodeWorker::incCount(){
    if(++m_cnt > m_dropEveryXFrame){
        m_cnt = 1;
    }
}
