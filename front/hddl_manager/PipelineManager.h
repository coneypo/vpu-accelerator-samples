#ifndef _PIPELINEMANAGER_H_
#define _PIPELINEMANAGER_H_

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include <boost/asio.hpp>

#include "Pipeline.h"

namespace hddl {

class PipelineManager {
public:
    PipelineManager();
    ~PipelineManager() = default;

    PipelineManager(const PipelineManager&) = delete;
    PipelineManager& operator=(const PipelineManager&) = delete;

    PipelineStatus removeAll();
    std::vector<int> getAll();

    PipelineStatus addPipeline(int id, std::string launch, std::string config);
    PipelineStatus deletePipeline(int id);
    PipelineStatus modifyPipeline(int id, std::string config);
    PipelineStatus playPipeline(int id);
    PipelineStatus stopPipeline(int id);
    PipelineStatus pausePipeline(int id);

private:
    /*
     * If NOT_EXIST, remove Pipeline id from map.
     * This function should be called while m_mapMutex locked.
     */
    void cleanupPipeline(int id, PipelineStatus status);

    void createSocket();
    void accept();
    void registerPipelineConnection();

    using Acceptor = std::unique_ptr<boost::asio::local::stream_protocol::acceptor>;
    using Socket = std::unique_ptr<boost::asio::local::stream_protocol::socket>;
    Acceptor m_acceptor;
    boost::asio::io_service m_ioContext;
    Socket m_tempSocket;
    std::mutex m_socketMutex;
    std::array<char, 1024> m_buffer;

    using Map = std::unordered_map<int, std::unique_ptr<Pipeline>>;
    std::mutex m_mapMutex;
    Map m_map;
};

}

#endif // _PIPELINEMANAGER_H_
