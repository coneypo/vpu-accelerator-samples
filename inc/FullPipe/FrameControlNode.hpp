#ifndef FRAME_CONTROL_NODE_HPP
#define FRAME_CONTROL_NODE_HPP

#include <hvaPipeline.hpp>
#include <string>
#include <common.hpp>
#include <unordered_map>

class FrameControlNode : public hva::hvaNode_t{
public:
    struct Config{
        unsigned dropEveryXFrame;
        unsigned dropXFrame;
    };

    // FrameControlNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, unsigned dropXFrame, unsigned dropEveryXFrame);
    FrameControlNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const Config& config);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    unsigned m_dropEveryXFrame;
    unsigned m_dropXFrame;
};

class FrameControlNodeWorker : public hva::hvaNodeWorker_t{
public:
    FrameControlNodeWorker(hva::hvaNode_t* parentNode, unsigned dropXFrame, unsigned dropEveryXFrame);

    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

private:
    void incCount(unsigned streamIdx);

    unsigned m_dropEveryXFrame;
    unsigned m_dropXFrame;
    // unsigned m_cnt;
    std::unordered_map<unsigned, unsigned> m_cntMap;
};

#endif //#ifndef FRAME_CONTROL_NODE_HPP