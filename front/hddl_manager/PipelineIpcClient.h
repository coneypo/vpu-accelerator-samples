#ifndef _PIPELINEIPCCLIENT_H_
#define _PIPELINEIPCCLIENT_H_

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <list>

#include "PipelineStatus.h"
#include "hddl_message.pb.h"

namespace hddl {

class PipelineIpcClient {
public:
    using Socket = std::unique_ptr<boost::asio::local::stream_protocol::socket>;
    using Ptr = std::unique_ptr<PipelineIpcClient>;

    PipelineIpcClient(int pipeId, Socket socket);
    ~PipelineIpcClient() = default;

    PipelineIpcClient(const PipelineIpcClient&) = delete;
    PipelineIpcClient& operator=(const PipelineIpcClient&) = delete;

    void readResponse();

    PipelineStatus create(std::string launch, std::string config);
    PipelineStatus modify(std::string config);
    PipelineStatus destroy();
    PipelineStatus play();
    PipelineStatus stop();
    PipelineStatus pause();

private:
    struct Request{
        MsgRequest reqMsg;
        std::condition_variable responseReceived;
        std::mutex mutex;
        std::unique_ptr<MsgResponse> rspMsg;
    };

    std::shared_ptr<Request> createRequest(MsgReqType type);
    std::unique_ptr<MsgResponse> sendRequestWaitResponse(std::shared_ptr<Request>& request);
    std::unique_ptr<MsgResponse> waitResponse(std::shared_ptr<Request>& request);
    bool sendRequest(MsgRequest& request);

    void parseResponse(boost::system::error_code ec);
    void handleResponse(std::unique_ptr<MsgResponse> response);
    std::shared_ptr<Request> fetchRequestBySeqNo(uint64_t seqNo);
    void onResponseReceived(std::shared_ptr<Request>& request, std::unique_ptr<MsgResponse> response);

    std::mutex m_requestSentMutex;
    std::list<std::shared_ptr<Request>> m_reqSentList;

    uint32_t m_length;
    std::array<char, 1024> m_buffer;
    Socket m_socket;
    std::mutex m_socketMutex;
    uint64_t m_seq_no;
    int m_pipeId;
    int m_timeout;
};
}

#endif
