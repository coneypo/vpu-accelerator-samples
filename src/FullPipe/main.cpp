#include <iostream>
#include <memory>
#include <hvaPipeline.hpp>
#include <chrono>
#include <thread>
#include <jsonParser.hpp>
#include <string>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem.hpp>
#include <future>
#include <jpeg_enc_node.hpp>
#include <FakeDelayNode.hpp>
#include <FrameControlNode.hpp>
#include <ipc.h>
#include <GstPipeContainer.hpp>
#include <common.hpp>
#include <mutex>
#include <condition_variable>
#include <boost/algorithm/string.hpp>
#include "hddl2plugin_helper.hpp"
#include "InferNode.hpp"

#define STREAMS 1
#define MAX_STREAMS 64
#define GUI_INTEGRATION
#define USE_FAKE_IE_NODE
using ms = std::chrono::milliseconds;

#ifdef GUI_INTEGRATION
#include <Sender.hpp>
#endif 

enum ControlMessage{
    EMPTY = 0,
    ADDR_RECVED,
    STOP_RECVED
};

std::mutex g_mutex;
std::condition_variable g_cv;

static std::string g_detNetwork;
static std::string g_clsNetwork;
static std::string g_videoFile;
static unsigned g_dropEveryXFrameDec;
static unsigned g_dropXFrameDec;
static unsigned g_dropEveryXFrameFRC;
static unsigned g_dropXFrameFRC;

bool checkValidNetFile(std::string filepath){
    std::string::size_type suffix_pos = filepath.find(".blob");
    if(suffix_pos == std::string::npos){
        std::cout<<"Invalid network suffix. Expect *.xml"<<std::endl;
        return false;
    }
    // std::string binFilepath = filepath.replace(suffix_pos, filepath.length(), ".bin");

    auto p = boost::filesystem::path(filepath);
    if(!boost::filesystem::exists(p) || !boost::filesystem::is_regular_file(p)){
        std::cout<<"File "<<filepath<<" does not exists"<<std::endl;
        return false;
    }

    // auto p_bin = boost::filesystem::path(binFilepath);
    // if(!boost::filesystem::exists(p_bin) || !boost::filesystem::is_regular_file(p_bin)){
    //     std::cout<<"File "<<binFilepath<<" does not exists"<<std::endl;
    //     return false;
    // }

    return true;
}

bool checkValidVideo(std::string filepath){
    auto p = boost::filesystem::path(filepath);
    if(!boost::filesystem::exists(p) || !boost::filesystem::is_regular_file(p)){
        std::cout<<"File "<<filepath<<" does not exists"<<std::endl;
        return false;
    }
    return true;
}

struct PipelineConfig_t{
    unsigned numOfStreams;
    // std::string unixSocket[MAX_STREAMS];
    std::vector<std::string> unixSocket;
};

int receiveRoutine(const char* socket_address, ControlMessage* ctrlMsg, PipelineConfig_t* config)
{
    auto poller = HddlUnite::Poller::create();
    auto connection = HddlUnite::Connection::create(poller);
    std::cout<<"Set socket to listening"<<std::endl;
    if (!connection->listen(socket_address)) {
        return -1;
    }

    bool running = true;

    while (running) {
        auto event = poller->waitEvent(10);
        switch (event.type) {
            case HddlUnite::Event::Type::CONNECTION_IN:
                connection->accept();
                std::cout<<"Incoming connection accepted"<<std::endl;
                break;
            case HddlUnite::Event::Type::MESSAGE_IN: {

                int length = 0;
                auto& data_connection = event.connection;
                std::string message;
                {
                    std::lock_guard<std::mutex> autoLock(data_connection->getMutex());

                    if (!data_connection->read(&length, sizeof(length))) {
                        break;
                    }
                    std::cout<<"Length received is "<<length<<std::endl;

                    if (length <= 0) {
                        break;
                    }

                    message = std::string(static_cast<size_t>(length), ' ');
                    if (!data_connection->read(&message[0], length)) {
                        break;
                    }
                    std::cout<<"message received is "<<message<<std::endl;
                }

                if(message=="STOP"){
                    /* to-do: stop pipeline*/
                }
                else{
                    /* start pipeline */
                    // std::vector<std::string> sockets;
                    // config->unixSocket.reserve();
                    {
                        std::lock_guard<std::mutex> lg(g_mutex);
                        boost::split(config->unixSocket, message, boost::is_any_of(","));
                        config->numOfStreams = config->unixSocket.size();
                        // for(unsigned i = 0; i< config->numOfStreams; ++i){
                        //     config->unixSocket[i] = sockets[i];
                        // }
                        *ctrlMsg = ControlMessage::ADDR_RECVED;
                        std::cout<<"Control message set to addr_recved"<<std::endl;
                    }
                    g_cv.notify_all();
                    std::cout<<"Going to stop receive routine after 5 s"<<std::endl;
                    std::this_thread::sleep_for(ms(5000));
                    running = false;
                }
                break;
            }
            case HddlUnite::Event::Type::CONNECTION_OUT:
                std::cout<<"Going to stop receive routine after 5 s"<<std::endl;
                std::this_thread::sleep_for(ms(5000));
                running = false;
                /* stop pipeline */
                // {
                //     std::lock_guard<std::mutex> lg(g_mutex);
                //     *ctrlMsg = ControlMessage::STOP_RECVED;
                // }
                // g_cv.notify_one();
                break;

            default:
                break;
        }
    }
}


int main(){

    gst_init(0, NULL);

    JsonParser jsParser("config.json");
    std::string guiSocket;
    if(!jsParser.parse("GUI.Socket",guiSocket)){
        std::cout<<"No GUI Socket specified. Exit"<<std::endl;
        return 0;
    }
    if(!jsParser.parse("Decode.Video",g_videoFile)){
        std::cout<<"Error: No input video file specified in config.json"<<std::endl;
        return -1;
    }
    if(!jsParser.parse("Decode.DropXFrame",g_dropXFrameDec)){
        std::cout<<"Warning: No frame dropping count for decoder specified in config.json"<<std::endl;
        g_dropXFrameDec = 0;
    }
    if(!jsParser.parse("Decode.DropEveryXFrame",g_dropEveryXFrameDec)){
        g_dropEveryXFrameDec = 1024;
    }
    if(!jsParser.parse("FRC.DropXFrame",g_dropXFrameFRC)){
        std::cout<<"Warning: No frame dropping count for FRC Node specified in config.json"<<std::endl;
        g_dropXFrameFRC = 0;
    }
    if(!jsParser.parse("FRC.DropEveryXFrame",g_dropEveryXFrameFRC)){
        g_dropEveryXFrameFRC = 1024;
    }

    if(!jsParser.parse("Detection.Model",g_detNetwork)){
        std::cout<<"Error: No detection model specified in config.json. Use default network path"<<std::endl;
        return 0;
    }
    if(!jsParser.parse("Classification.Model",g_clsNetwork)){
        std::cout<<"Error: No classification model specified in config.json. Use default network path"<<std::endl;
        return 0;
    }

    if(!checkValidNetFile(g_detNetwork) || !checkValidNetFile(g_clsNetwork) || !checkValidVideo(g_videoFile)){
        return -1;
    }

#ifdef GUI_INTEGRATION
    PipelineConfig_t config;
    config.numOfStreams = 0;
    // config.unixSocket = "";
    ControlMessage ctrlMsg = ControlMessage::EMPTY;
#endif
    GstPipeContainer::Config decConfig;
    decConfig.filename = g_videoFile;
    decConfig.enableFpsCounting = true;
    decConfig.dropEveryXFrame = g_dropEveryXFrameDec;
    decConfig.dropXFrame = g_dropXFrameDec;

#ifdef GUI_INTEGRATION
    std::thread t(receiveRoutine, guiSocket.c_str(), &ctrlMsg, &config);
#endif

    std::mutex WIDMutex;
    std::condition_variable WIDCv;
    unsigned WIDReadyCnt = 0;
    // std::promise<uint64_t> WIDPromise;
    // std::future<uint64_t> WIDFuture = WIDPromise.get_future();
    hva::hvaPipeline_t pl;

#ifdef GUI_INTEGRATION
    // bool ready = configFuture.get();
    {
        std::unique_lock<std::mutex> lk(g_mutex);
        if(config.numOfStreams == 0){
            g_cv.wait(lk,[&](){ return ctrlMsg == ControlMessage::ADDR_RECVED;});
            std::cout<<"Control message addr_recved received and cleared"<<std::endl;
            ctrlMsg = ControlMessage::EMPTY;
        }
    }
    std::vector<std::thread*> vTh;
    vTh.reserve(config.numOfStreams);
    std::vector<GstPipeContainer*> vCont(config.numOfStreams);
    std::vector<uint64_t> vWID(config.numOfStreams, 0); 
    // std::cout<<" Unix Socket addr received is "<<config.unixSocket<<std::endl;
    std::this_thread::sleep_for(ms(1000));
#endif
    for(unsigned i = 0; i < config.numOfStreams; ++i){
        // std::cout<<"starting thread "<<i<<std::endl;
        vTh.push_back(new std::thread([&, i](){
                    // GstPipeContainer cont(i);
                    vCont[i] = new GstPipeContainer(i);
                    uint64_t WID = 0;
                    if(vCont[i]->init(decConfig, WID) != 0 || WID == 0){
                        std::cout<<"Fail to init cont!"<<std::endl;
                        return;
                    }

                    // WIDPromise.set_value(WID);
                    vWID[i] = WID;
                    {
                        std::lock_guard<std::mutex> WIDLg(WIDMutex);
                        ++WIDReadyCnt;
                    }
                    WIDCv.notify_all();

                    std::this_thread::sleep_for(ms(2000));
                    std::cout<<"Dec source start feeding."<<std::endl;

                    std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
                    while(vCont[i]->read(blob)){
                        int* fd = blob->get<int, VideoMeta>(0)->getPtr();
                        std::cout<<"FD received is "<<*fd<<"with streamid "<<blob->streamId<<" and frameid "<<blob->frameId<<std::endl;

                        pl.sendToPort(blob,"detNode",0,ms(0));

                        blob.reset(new hva::hvaBlob_t());
                        std::cout<<"Sent fd "<<*fd<<" completed"<<std::endl;
                    }

                    {
                    std::lock_guard<std::mutex> lg(g_mutex);
                    ctrlMsg = ControlMessage::STOP_RECVED;
                    }
                    g_cv.notify_all();

                    std::cout<<"Finished"<<std::endl;
                    return;
                }));
    }

    // uint64_t WID = WIDFuture.get();
    std::unique_lock<std::mutex> WIDLock(WIDMutex);
    WIDCv.wait(WIDLock,[&](){ return WIDReadyCnt == config.numOfStreams;});
    WIDLock.unlock();

    std::cout<<"All WIDs received. Start pipeline with "<<config.numOfStreams<<" streams."<<std::endl;
#ifdef USE_FAKE_IE_NODE
    auto& detNode = pl.setSource(std::make_shared<FakeDelayNode>(1,1,2, "detection"), "detNode");
#else
    auto &detNode = pl.setSource(std::make_shared<InferNode>(1, 1, 1, WID, g_detNetwork, "detection", 
                                                             &HDDL2pluginHelper_t::postprocYolotinyv2_u8), "detNode");
#endif
    auto &FRCNode = pl.addNode(std::make_shared<FrameControlNode>(1, 1, 0, g_dropXFrameFRC, g_dropEveryXFrameFRC), "FRCNode");

#ifdef GUI_INTEGRATION

#ifdef USE_FAKE_IE_NODE
    auto& clsNode = pl.addNode(std::make_shared<FakeDelayNode>(1,2,2,"classification"), "clsNode");
#else //#ifdef USE_FAKE_IE_NODE
    auto &clsNode = pl.addNode(std::make_shared<InferNode>(1, 2, 1, WID, g_clsNetwork, "classification",
                                                           &HDDL2pluginHelper_t::postprocResnet50_u8), "clsNode");
#endif //#ifdef USE_FAKE_IE_NODE
    if(config.numOfStreams > 1){
        pl.addNode(std::make_shared<SenderNode>(1,1,2,config.unixSocket), "sendNode");
    }
    else{
        pl.addNode(std::make_shared<SenderNode>(1,1,1,config.unixSocket), "sendNode");
    }
    // auto& sendNode = pl.addNode(std::make_shared<SenderNode>(1,1,2,config.unixSocket), "sendNode");

#else //#ifdef GUI_INTEGRATION

#ifdef USE_FAKE_IE_NODE
    auto& clsNode = pl.addNode(std::make_shared<FakeDelayNode>(1,1,1,"classification"), "clsNode");
#else //#ifdef USE_FAKE_IE_NODE
    auto &clsNode = pl.addNode(std::make_shared<InferNode>(1, 1, 1, WID, g_clsNetwork, "classification",
                                                           &HDDL2pluginHelper_t::postprocResnet50_u8), "clsNode");
#endif //#ifdef USE_FAKE_IE_NODE

#endif //#ifdef GUI_INTEGRATION
    auto& jpegNode = pl.addNode(std::make_shared<JpegEncNode>(1,1,config.numOfStreams,vWID), "jpegNode");

    if(config.numOfStreams > 1){
        hva::hvaBatchingConfig_t batchingConfig;
        batchingConfig.batchingPolicy = hva::hvaBatchingConfig_t::BatchingWithStream;
        batchingConfig.batchSize = 1;
        batchingConfig.streamNum = config.numOfStreams;
        batchingConfig.threadNumPerBatch = 1;

        jpegNode.configBatch(batchingConfig);
    }

    pl.linkNode("detNode", 0, "FRCNode", 0);
    pl.linkNode("FRCNode", 0, "clsNode", 0);
    pl.linkNode("clsNode", 0, "jpegNode", 0);
#ifdef GUI_INTEGRATION
    pl.linkNode("clsNode", 1, "sendNode", 0);
#endif

    pl.prepare();

    std::cout<<"\nPipeline Start: "<<std::endl;
    pl.start();

    do{
        std::unique_lock<std::mutex> lk(g_mutex);
        g_cv.wait(lk,[&](){ return ControlMessage::STOP_RECVED == ctrlMsg;});
        std::cout<<"Control message stop_recved received and cleared"<<std::endl;
        ctrlMsg = ControlMessage::EMPTY;

        std::this_thread::sleep_for(ms(2000)); //temp WA to let last decoded framed finish

        std::cout<<"Going to stop pipeline."<<std::endl;

        pl.stop();

        for(unsigned i =0; i < config.numOfStreams; ++i){
            vCont[i]->stop();
            delete vCont[i];
        }

        std::this_thread::sleep_for(ms(1000));

        for(unsigned i =0; i < config.numOfStreams; ++i){
            std::cout<<"Going to join "<<i<<"th dec source"<<std::endl;
            vTh[i]->join();
        }

        std::cout<<"\nDec source joined "<<std::endl;

#ifdef GUI_INTEGRATION
        t.join();
#endif
        break;

    }while(true);

    return 0;

}
