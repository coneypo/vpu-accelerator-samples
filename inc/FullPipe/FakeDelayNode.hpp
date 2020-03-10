#ifndef FAKEDELAYNODE_HPP
#define FAKEDELAYNODE_HPP

#include <hvaPipeline.hpp>
#include <string>
#include <common.hpp>

class FakeDelayNode : public hva::hvaNode_t{
public:
    FakeDelayNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, std::string mode);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    std::string m_mode;

};

class FakeDelayNodeWorker : public hva::hvaNodeWorker_t{
public:
    FakeDelayNodeWorker(hva::hvaNode_t* parentNode, std::string mode);

    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

private:

    std::string m_mode;
};

#endif //#ifndef FAKEDELAYNODE_HPP