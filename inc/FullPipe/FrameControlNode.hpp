#ifndef FRAME_CONTROL_NODE_HPP
#define FRAME_CONTROL_NODE_HPP

#include <hvaPipeline.hpp>
#include <string>
#include <common.hpp>

class FrameControlNode : public hva::hvaNode_t{
public:
    FrameControlNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, unsigned dropEveryXFrame);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    unsigned m_dropEveryXFrame;

};

class FrameControlNodeWorker : public hva::hvaNodeWorker_t{
public:
    FrameControlNodeWorker(hva::hvaNode_t* parentNode, unsigned dropEveryXFrame);

    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

private:

    unsigned m_dropEveryXFrame;
    unsigned m_cnt;
};

#endif //#ifndef FRAME_CONTROL_NODE_HPP