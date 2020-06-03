#ifndef IMGFRAME_CONTROL_NODE_HPP
#define IMGFRAME_CONTROL_NODE_HPP

#include <hvaPipeline.hpp>
#include <string>
#include <common.hpp>
#include <unordered_map>

class ImgFrameControlNode : public hva::hvaNode_t{
public:
    struct Config{
        unsigned dropEveryXFrame;
        unsigned dropXFrame;
    };

    // FrameControlNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, unsigned dropXFrame, unsigned dropEveryXFrame);
    ImgFrameControlNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum/*, const Config& config*/);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    unsigned m_dropEveryXFrame;
    unsigned m_dropXFrame;
};

class ImgFrameControlNodeWorker : public hva::hvaNodeWorker_t{
public:
    ImgFrameControlNodeWorker(hva::hvaNode_t* parentNode, unsigned dropXFrame, unsigned dropEveryXFrame);

    /**
    * @brief Main frame dropping logic where this node manages on each frame's drop meta
    *
    * @param batchIdx batch index assigned by framework
    * @return void
    *
    */
    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

private:
    void incCount(unsigned streamIdx);

    unsigned m_dropEveryXFrame;
    unsigned m_dropXFrame;
    std::unordered_map<unsigned, unsigned> m_cntMap;
};

#endif //#ifndef IMGFRAME_CONTROL_NODE_HPP
