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
        m_dropEveryXFrame(dropEveryXFrame), m_dropXFrame(dropXFrame), m_cnt(0u){

}

void FrameControlNodeWorker::process(std::size_t batchIdx){
    std::vector<std::shared_ptr<hva::hvaBlob_t>> vInput= hvaNodeWorker_t::getParentPtr()->getBatchedInput(batchIdx, std::vector<size_t> {0});

    if(vInput.size() != 0){
        std::cout<<"FRC with tid "<<syscall(SYS_gettid)<<" received blob with streamid "<<vInput[0]->streamId<<" and frameid "<<vInput[0]->frameId<<std::endl;
        auto ptrBufInfer = vInput[0]->get<int, InferMeta>(0);
        InferMeta* ptrInferMeta = ptrBufInfer->getMeta();
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
        ptrInferMeta->drop = drop;
        sendOutput(vInput[0], 0, ms(0));
        incCount();
        std::cout<<"FRC sent blob with streamid "<<vInput[0]->streamId<<" and frameid "<<vInput[0]->frameId<<std::endl;
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

void FrameControlNodeWorker::incCount(){
    if(++m_cnt > m_dropEveryXFrame){
        m_cnt = 1;
    }
}