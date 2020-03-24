#ifndef SENDER_HPP
#define SENDER_HPP

#include <hvaPipeline.hpp>
#include <string>
#include <infermetasender.hpp>
#include <common.hpp>
#include <atomic>
#include <unordered_map>

class SenderNode : public hva::hvaNode_t{
public:
    SenderNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const std::vector<std::string>& unixSocket);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

    std::string getUnixSocket(unsigned index) const;

private:
    std::vector<std::string> m_unixSocket;
    unsigned m_numOfSockets;
    unsigned m_numOfWorkers;
    mutable std::atomic<unsigned> m_workerIdx;
};

class SenderNodeWorker : public hva::hvaNodeWorker_t{
public:
    SenderNodeWorker(hva::hvaNode_t* parentNode, const std::unordered_map<unsigned, std::string>& unixSocket);

    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

    virtual void deinit() override;

private:
    std::unordered_map<unsigned, InferMetaSender*> m_sender;
    std::unordered_map<unsigned, std::string> m_unixSocket; 
};


#endif //#ifndef SENDER_HPP