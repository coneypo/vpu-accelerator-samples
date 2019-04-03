#ifndef _PIPELINEIPC_H_
#define _PIPELINEIPC_H_

#include <array>
#include <boost/asio.hpp>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "PipelineIpcClient.h"
#include "PipelineStatus.h"
#include "hddl_message.pb.h"

namespace hddl {

class PipelineIPC {
public:
    PipelineIPC(const PipelineIPC&) = delete;
    PipelineIPC& operator=(const PipelineIPC&) = delete;

    void init(int socketId);
    void uninit();

    std::string getSocketName() { return m_socketName; }
    PipelineIpcClient::Ptr getIpcClient(int pipeId, int timeout_sec);
    void cleanupIpcClient(int pipeId);

    static PipelineIPC& getInstance()
    {
        static PipelineIPC instance;
        return instance;
    }

private:
    using Acceptor = std::unique_ptr<boost::asio::local::stream_protocol::acceptor>;
    using Socket = std::unique_ptr<boost::asio::local::stream_protocol::socket>;

    PipelineIPC();
    ~PipelineIPC() = default;

    void createSocket(int socketId);
    void accept();
    void registerPipelineConnection();

    std::string m_socketName = "/tmp/hddl_manager.sock";

    Acceptor m_acceptor;
    boost::asio::io_service m_ioContext;
    Socket m_tempSocket;
    std::array<char, 1024> m_buffer;

    std::mutex m_mapMutex;
    std::map<int, std::promise<PipelineIpcClient::Ptr>> m_map;
};
}

#endif // _PIPELINEIPC_H_
