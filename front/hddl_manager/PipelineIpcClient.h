#ifndef _PIPELINEIPCCLIENT_H_
#define _PIPELINEIPCCLIENT_H_

#include <boost/asio.hpp>
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

    PipelineStatus create(std::string launch, std::string config);
    PipelineStatus modify(std::string config);
    PipelineStatus destroy();
    PipelineStatus play();
    PipelineStatus stop();
    PipelineStatus pause();

private:
    MsgRequest createRequest(MsgReqType type);
    std::unique_ptr<MsgResponse> sendRequest(const MsgRequest& request);
    Socket m_socket;
    uint64_t m_seq_no;
    int m_pipeId;
};
}

#endif
