#ifndef IMGSENDER_HPP
#define IMGSENDER_HPP

#include <hvaPipeline.hpp>
#include <string>
#include <infermetasender.hpp>
#include <common.hpp>
#include <atomic>
#include <unordered_map>

// sender class communicates with GUI through IPC
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

    /**
    * @brief wrap every input and send to GUI's IPC socket
    *
    * @param batchIdx batch index assigned by framework
    * @return void
    *
    */
    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

    virtual void deinit() override;

private:
    std::unordered_map<unsigned, InferMetaSender*> m_sender;
    std::unordered_map<unsigned, std::string> m_unixSocket; 
    uint64_t m_cntFrame {0ul};
    float m_durationAve;
    float m_fps {0.0f};
    std::chrono::time_point<std::chrono::steady_clock> senderTimeStart;
};


#endif //#ifndef SENDER_HPP
