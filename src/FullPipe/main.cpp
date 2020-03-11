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

#define STREAMS 1
#define GUI_INTEGRATION
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
static unsigned g_dropEveryXFrame;
static unsigned g_dropXFrame;

bool checkValidNetFile(std::string filepath){
    std::string::size_type suffix_pos = filepath.find(".xml");
    if(suffix_pos == std::string::npos){
        std::cout<<"Invalid network suffix. Expect *.xml"<<std::endl;
        return false;
    }
    std::string binFilepath = filepath.replace(suffix_pos, filepath.length(), ".bin");

    auto p = boost::filesystem::path(filepath);
    if(!boost::filesystem::exists(p) || !boost::filesystem::is_regular_file(p)){
        std::cout<<"File "<<filepath<<" does not exists"<<std::endl;
        return false;
    }

    auto p_bin = boost::filesystem::path(binFilepath);
    if(!boost::filesystem::exists(p_bin) || !boost::filesystem::is_regular_file(p_bin)){
        std::cout<<"File "<<binFilepath<<" does not exists"<<std::endl;
        return false;
    }

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
    std::string unixSocket;
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
        auto event = poller->waitEvent(100);
        switch (event.type) {
        case HddlUnite::Event::Type::CONNECTION_IN:
            connection->accept();
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
                {
                    std::lock_guard<std::mutex> lg(g_mutex);
                    config->unixSocket = message;
                    *ctrlMsg = ControlMessage::ADDR_RECVED;
                }
                g_cv.notify_one();
                /* start pipeline */
            }
            break;
        }
        case HddlUnite::Event::Type::CONNECTION_OUT:
            running = false;
            /* stop pipeline */
            {
                std::lock_guard<std::mutex> lg(g_mutex);
                *ctrlMsg = ControlMessage::STOP_RECVED;
            }
            g_cv.notify_one();
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
    if(!jsParser.parse("Decode.DropXFrame",g_dropXFrame)){
        std::cout<<"Warning: No frame dropping count specified in config.json"<<std::endl;
        g_dropXFrame = 0;
    }
    if(!jsParser.parse("Decode.DropEveryXFrame",g_dropEveryXFrame)){
        g_dropEveryXFrame = 1024;
    }

    if(!jsParser.parse("Detection.Model",g_detNetwork)){
        std::cout<<"No detection model specified in config.json. Use default network path"<<std::endl;
        g_detNetwork = "yolov2_tiny_od_yolo_IR_fp32.xml";
    }
    if(!jsParser.parse("Classification.Model",g_clsNetwork)){
        std::cout<<"No classification model specified in config.json. Use default network path"<<std::endl;
        g_clsNetwork = "ResNet-50_fp32.xml";
    }

    if(!checkValidNetFile(g_detNetwork) || !checkValidNetFile(g_clsNetwork) || !checkValidVideo(g_videoFile)){
        return -1;
    }

#ifdef GUI_INTEGRATION
    PipelineConfig_t config;
    config.unixSocket = "";
    ControlMessage ctrlMsg = ControlMessage::EMPTY;
#endif
    GstPipeContainer::Config decConfig;
    decConfig.filename = g_videoFile;
    decConfig.dropEveryXFrame = g_dropEveryXFrame;

#ifdef GUI_INTEGRATION
    std::thread t(receiveRoutine, guiSocket.c_str(), &ctrlMsg, &config);
#endif

    std::vector<std::thread*> vTh;
    std::vector<GstPipeContainer*> vCont;
    vTh.reserve(STREAMS);
    vCont.reserve(STREAMS);
    std::promise<uint64_t> WIDPromise;
    std::future<uint64_t> WIDFuture = WIDPromise.get_future();
    hva::hvaPipeline_t pl;

#ifdef GUI_INTEGRATION
    // bool ready = configFuture.get();
    {
        std::unique_lock<std::mutex> lk(g_mutex);
        if(config.unixSocket == ""){
            g_cv.wait(lk,[&](){ return ctrlMsg == ControlMessage::ADDR_RECVED;});
            ctrlMsg = ControlMessage::EMPTY;
        }
    }
    std::cout<<" Unix Socket addr received is "<<config.unixSocket<<std::endl;
    std::this_thread::sleep_for(ms(5000));
#endif
    for(unsigned i = 0; i < STREAMS; ++i){
        // std::cout<<"starting thread "<<i<<std::endl;
        vTh.push_back(new std::thread([&, i](){
                    // GstPipeContainer cont(i);
                    vCont[i] = new GstPipeContainer(i);
                    uint64_t WID = 0;
                    if(vCont[i]->init(decConfig, WID) != 0 || WID == 0){
                        std::cout<<"Fail to init cont!"<<std::endl;
                        return;
                    }

                    WIDPromise.set_value(WID);

                    std::this_thread::sleep_for(ms(2000));
                    std::cout<<"Dec source start feeding."<<std::endl;

                    std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
                    while(vCont[i]->read(blob)){
                        int* fd = blob->get<int, VideoMeta>(0)->getPtr();
                        std::cout<<"FD received is "<<*fd<<std::endl;

                        pl.sendToPort(blob,"detNode",0,ms(0));

                        blob.reset(new hva::hvaBlob_t());
                    }

                    {
                    std::lock_guard<std::mutex> lg(g_mutex);
                    ctrlMsg = ControlMessage::STOP_RECVED;
                    }
                    g_cv.notify_all();

                    // std::cout<<"Finished"<<std::endl;
                    return;
                }));
    }

    auto& detNode = pl.setSource(std::make_shared<FakeDelayNode>(1,1,1, "detection"), "detNode");
    auto& FRCNode = pl.addNode(std::make_shared<FrameControlNode>(1,1,0,1,4), "FRCNode");
    
#ifdef GUI_INTEGRATION
    auto& clsNode = pl.addNode(std::make_shared<FakeDelayNode>(1,2,1,"classification"), "clsNode");
    auto& sendNode = pl.addNode(std::make_shared<SenderNode>(1,1,1,config.unixSocket), "sendNode");
#else
    auto& clsNode = pl.addNode(std::make_shared<FakeDelayNode>(1,1,1,"classification"), "clsNode");
#endif
    uint64_t WID = WIDFuture.get();
    auto& jpegNode = pl.addNode(std::make_shared<JpegEncNode>(1,1,1,WID), "jpegNode");

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

        ctrlMsg = ControlMessage::EMPTY;
        for(unsigned i =0; i < STREAMS; ++i){
            vCont[i]->stop();
            delete vCont[i];
        }

        std::this_thread::sleep_for(ms(1000));

        std::cout<<"Going to stop pipeline."<<std::endl;

        pl.stop();

        for(unsigned i =0; i < STREAMS; ++i){
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
