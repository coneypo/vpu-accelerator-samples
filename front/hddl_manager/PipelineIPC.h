#ifndef _PIPELINEIPC_H_
#define _PIPELINEIPC_H_

#include <string>
#include <array>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <boost/asio.hpp>
#include "hddl_message.pb.h"
#include "PipelineStatus.h"


namespace hddl {

class PipelineIPC {
public:
    PipelineIPC(const PipelineIPC&) = delete;
    PipelineIPC& operator=(const PipelineIPC&) = delete;

    void init(int socketId);
    void uninit();

    std::string getSocketName() { return m_socketName; }

    PipelineStatus create(int pipeId, std::string launch, std::string config);
    PipelineStatus modify(int pipeId, std::string config);
    PipelineStatus destroy(int pipeId);
    PipelineStatus play(int pipeId);
    PipelineStatus stop(int pipeId);
    PipelineStatus pause(int pipeId);

    static PipelineIPC& getInstance()
    {
        static PipelineIPC instance;
        return instance;
    }

private:
    PipelineIPC();
    ~PipelineIPC() = default;

    void createSocket(int socketId);
    void accept();
    void registerPipelineConnection();

    MsgRequest createRequest(int pipeId, MsgReqType type);
    std::unique_ptr<MsgResponse> sendRequest(int pipeId, const MsgRequest& request);

    std::string m_socketName = "/tmp/hddl_manager.sock";

    using Acceptor = std::unique_ptr<boost::asio::local::stream_protocol::acceptor>;
    using Socket = std::unique_ptr<boost::asio::local::stream_protocol::socket>;
    Acceptor m_acceptor;
    boost::asio::io_service m_ioContext;
    Socket m_tempSocket;
    std::array<char, 1024> m_buffer;

    std::mutex m_mapMutex;
    std::unordered_map<int, Socket> m_pipes;
    std::unordered_map<int, uint64_t> m_seq_no;
};

}

#endif // _PIPELINEIPC_H_
