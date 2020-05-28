#ifndef IMGSENDER_HPP
#define IMGSENDER_HPP

#include <hvaPipeline.hpp>
#include <string>
#include <infermetasender.hpp>
#include <common.hpp>
#include <atomic>
#include <unordered_map>

class ImgSenderNode : public hva::hvaNode_t{
public:
	ImgSenderNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, const std::vector<std::string>& unixSocket);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

    std::string getUnixSocket(unsigned index) const;

private:
    std::vector<std::string> m_unixSocket;
    unsigned m_numOfSockets;
    unsigned m_numOfWorkers;
    mutable std::atomic<unsigned> m_workerIdx;
};

class ImgSenderNodeWorker : public hva::hvaNodeWorker_t{
public:
	ImgSenderNodeWorker(hva::hvaNode_t* parentNode, const std::unordered_map<unsigned, std::string>& unixSocket);

    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

    virtual void deinit() override;

private:
    std::unordered_map<unsigned, InferMetaSender*> m_sender;
    std::unordered_map<unsigned, std::string> m_unixSocket; 
    float m_durationAve;
};


#endif //#ifndef SENDER_HPP
