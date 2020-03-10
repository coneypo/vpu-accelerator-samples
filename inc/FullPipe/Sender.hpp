#ifndef SENDER_HPP
#define SENDER_HPP

#include <hvaPipeline.hpp>
#include <string>
#include <infermetasender.hpp>
#include <common.hpp>

class SenderNode : public hva::hvaNode_t{
public:
    SenderNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const std::string& unixSocket);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

    std::string getUnixSocket() const;

private:
    std::string m_unixSocket;
};

class SenderNodeWorker : public hva::hvaNodeWorker_t{
public:
    SenderNodeWorker(hva::hvaNode_t* parentNode);

    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

private:
    InferMetaSender m_sender;  
};


#endif //#ifndef SENDER_HPP