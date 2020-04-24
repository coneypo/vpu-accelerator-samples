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
#include "InferNode_unite.hpp"
#include <PipelineConfig.hpp>
#include "object_tracking_node.hpp"
#include <validationDumpNode.hpp>

#define STREAMS 1
#define MAX_STREAMS 64
#define GUI_INTEGRATION
// #define USE_FAKE_IE_NODE
// #define USE_OBJECT_TRACKING
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

struct SocketsConfig_t{
    unsigned numOfStreams;
    // std::string unixSocket[MAX_STREAMS];
    std::vector<std::string> unixSocket;
};

int receiveRoutine(const char* socket_address, ControlMessage* ctrlMsg, SocketsConfig_t* config)
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
                        for (unsigned i = 0; i < config->unixSocket.size();++i){
                            std::cout << "Value of config[" << i << "]: " << config->unixSocket[i] << std::endl;
                        }
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
    return 0;
}


int main(){

    gst_init(0, NULL);

    PipelineConfigParser configParser;
    if(!configParser.parse("config.json")){
        std::cout<<"Failed to parse config.json"<<std::endl;
        return 0;
    }

    PipelineConfig config = configParser.get();

    if(!checkValidNetFile(config.detConfig.model) || !checkValidNetFile(config.clsConfig.model)){
        return -1;
    }
    for(const auto& decConfigItem: config.vDecConfig){
        if(!checkValidVideo(decConfigItem.filename))
            return -1;
    }

#ifdef GUI_INTEGRATION
    SocketsConfig_t sockConfig;
    sockConfig.numOfStreams = 0;
    ControlMessage ctrlMsg = ControlMessage::EMPTY;
#endif

#ifdef GUI_INTEGRATION
    std::thread t(receiveRoutine, config.guiSocket.c_str(), &ctrlMsg, &sockConfig);
#endif

    std::mutex WIDMutex;
    std::condition_variable WIDCv;
    unsigned WIDReadyCnt = 0;
    hva::hvaPipeline_t pl;

#ifdef GUI_INTEGRATION
    {
        std::unique_lock<std::mutex> lk(g_mutex);
        if(sockConfig.numOfStreams == 0){
            g_cv.wait(lk,[&](){ return ctrlMsg == ControlMessage::ADDR_RECVED;});
            std::cout<<"Control message addr_recved received and cleared"<<std::endl;
            ctrlMsg = ControlMessage::EMPTY;
        }
    }
    std::vector<std::thread*> vTh;
    vTh.reserve(sockConfig.numOfStreams);
    std::vector<GstPipeContainer*> vCont(sockConfig.numOfStreams);
    std::vector<uint64_t> vWID(sockConfig.numOfStreams, 0); 
    std::this_thread::sleep_for(ms(1000));
#endif
    for(unsigned i = 0; i < sockConfig.numOfStreams; ++i){
        vTh.push_back(new std::thread([&, i](){
                    vCont[i] = new GstPipeContainer(i);
                    uint64_t WID = 0;
                    if(vCont[i]->init(config.vDecConfig[i], WID) != 0 || WID == 0){
                        std::cout<<"Fail to init cont!"<<std::endl;
                        return;
                    }

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

    std::unique_lock<std::mutex> WIDLock(WIDMutex);
    WIDCv.wait(WIDLock,[&](){ return WIDReadyCnt == sockConfig.numOfStreams;});
    WIDLock.unlock();

    std::cout<<"All WIDs received. Start pipeline with "<<sockConfig.numOfStreams<<" streams."<<std::endl;
#ifdef USE_FAKE_IE_NODE
    auto& detNode = pl.setSource(std::make_shared<FakeDelayNode>(1,1,2, "detection"), "detNode");
#else
    auto &detNode = pl.setSource(std::make_shared<InferNode>(1, 1, 1, vWID[0], config.detConfig.model, "detection", 
                                                             &HDDL2pluginHelper_t::postprocYolotinyv2_u8), "detNode");

    // auto& detNode = pl.setSource(std::make_shared<InferNode_unite>(1,1,sockConfig.numOfStreams, 
    // vWID, config.detConfig.model, "detection", 416*416*3, 13*13*125), "detNode");
    if(sockConfig.numOfStreams > 1){
        hva::hvaBatchingConfig_t batchingConfig;
        batchingConfig.batchingPolicy = hva::hvaBatchingConfig_t::BatchingWithStream;
        batchingConfig.batchSize = 1;
        batchingConfig.streamNum = sockConfig.numOfStreams;
        batchingConfig.threadNumPerBatch = 1;

        detNode.configBatch(batchingConfig);
    }
#endif

#ifndef USE_OBJECT_TRACKING
    auto &FRCNode = pl.addNode(std::make_shared<FrameControlNode>(1, 1, 0, config.FRCConfig), "FRCNode");
#else
    auto &trackingNode = pl.addNode(std::make_shared<ObjectTrackingNode>(1, 1, sockConfig.numOfStreams, 
    vWID, 0, "objectTracking", "zero_term_imageless"), "trackingNode");
    if(sockConfig.numOfStreams > 1){
        hva::hvaBatchingConfig_t batchingConfig;
        batchingConfig.batchingPolicy = hva::hvaBatchingConfig_t::BatchingWithStream;
        batchingConfig.batchSize = 1;
        batchingConfig.streamNum = sockConfig.numOfStreams;
        batchingConfig.threadNumPerBatch = 1;

        trackingNode.configBatch(batchingConfig);
    }
#endif

#ifdef GUI_INTEGRATION

#ifdef USE_FAKE_IE_NODE
    auto& clsNode = pl.addNode(std::make_shared<FakeDelayNode>(1,2,2,"classification"), "clsNode");
#else //#ifdef USE_FAKE_IE_NODE
#ifndef VALIDATION_DUMP
    auto& clsNode = pl.addNode(std::make_shared<InferNode_unite>(1,2,sockConfig.numOfStreams, 
    vWID, config.clsConfig.model, "classification", 224*224*3, 1000), "clsNode");
    if(sockConfig.numOfStreams > 1){
        hva::hvaBatchingConfig_t batchingConfig;
        batchingConfig.batchingPolicy = hva::hvaBatchingConfig_t::BatchingWithStream;
        batchingConfig.batchSize = 1;
        batchingConfig.streamNum = sockConfig.numOfStreams;
        batchingConfig.threadNumPerBatch = 1;

        clsNode.configBatch(batchingConfig);
    }
#else //#ifndef VALIDATION_DUMP
    auto& clsNode = pl.addNode(std::make_shared<InferNode_unite>(1,3,sockConfig.numOfStreams, 
    vWID, config.clsConfig.model, "classification", 224*224*3, 1000), "clsNode");
    if(sockConfig.numOfStreams > 1){
        hva::hvaBatchingConfig_t batchingConfig;
        batchingConfig.batchingPolicy = hva::hvaBatchingConfig_t::BatchingWithStream;
        batchingConfig.batchSize = 1;
        batchingConfig.streamNum = sockConfig.numOfStreams;
        batchingConfig.threadNumPerBatch = 1;

        clsNode.configBatch(batchingConfig);
    }
#endif //#ifndef VALIDATION_DUMP
#endif //#ifdef USE_FAKE_IE_NODE
    if(sockConfig.numOfStreams > 1){
        pl.addNode(std::make_shared<SenderNode>(1,1,sockConfig.numOfStreams,sockConfig.unixSocket), "sendNode");
    }
    else{
        pl.addNode(std::make_shared<SenderNode>(1,1,1,sockConfig.unixSocket), "sendNode");
    }

#else //#ifdef GUI_INTEGRATION

#ifdef USE_FAKE_IE_NODE
    auto& clsNode = pl.addNode(std::make_shared<FakeDelayNode>(1,1,1,"classification"), "clsNode");
#else //#ifdef USE_FAKE_IE_NODE
    auto &clsNode = pl.addNode(std::make_shared<InferNode>(1, 1, 1, WID, config.clsConfig.model, "classification",
                                                           &HDDL2pluginHelper_t::postprocResnet50_u8), "clsNode");
#endif //#ifdef USE_FAKE_IE_NODE

#endif //#ifdef GUI_INTEGRATION
    auto& jpegNode = pl.addNode(std::make_shared<JpegEncNode>(1,1,sockConfig.numOfStreams,vWID), "jpegNode");

    if(sockConfig.numOfStreams > 1){
        hva::hvaBatchingConfig_t batchingConfig;
        batchingConfig.batchingPolicy = hva::hvaBatchingConfig_t::BatchingWithStream;
        batchingConfig.batchSize = 1;
        batchingConfig.streamNum = sockConfig.numOfStreams;
        batchingConfig.threadNumPerBatch = 1;

        jpegNode.configBatch(batchingConfig);
    }

#ifdef VALIDATION_DUMP
    auto& validationDumpNode = pl.addNode(std::make_shared<ValidationDumpNode>(1,0,1,"Resnet"), "validationDumpNode");
#endif //#ifdef VALIDATION_DUMP

#ifndef USE_OBJECT_TRACKING
    pl.linkNode("detNode", 0, "FRCNode", 0);
    pl.linkNode("FRCNode", 0, "clsNode", 0);
#else
    pl.linkNode("detNode", 0, "trackingNode", 0);
    pl.linkNode("trackingNode", 0, "clsNode", 0);
#endif

    pl.linkNode("clsNode", 0, "jpegNode", 0);
#ifdef GUI_INTEGRATION
    pl.linkNode("clsNode", 1, "sendNode", 0);
#ifdef VALIDATION_DUMP
    pl.linkNode("clsNode", 2, "validationDumpNode", 0);
#endif //#ifdef VALIDATION_DUMP
#endif //#ifdef GUI_INTEGRATION

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

        for(unsigned i =0; i < sockConfig.numOfStreams; ++i){
            vCont[i]->stop();
            delete vCont[i];
        }

        std::this_thread::sleep_for(ms(1000));

        for(unsigned i =0; i < sockConfig.numOfStreams; ++i){
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
