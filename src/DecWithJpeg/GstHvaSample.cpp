#include <GstHvaSample.hpp>
#include <iostream>
#include <memory>
#include <hvaPipeline.hpp>
// #include <infer_node.hpp>
#include <chrono>
#include <thread>
#include <jsonParser.hpp>
#include <string>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem.hpp>
#include <future>

#define STREAMS 1

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

int main(){

    gst_init(0, NULL);

    JsonParser jsParser("config.json");
    std::string detNetwork, clsNetwork, videoFile;
    if(!jsParser.parse("Detection.Model",detNetwork)){
        std::cout<<"No detection model specified in config.json. Use default network path"<<std::endl;
        detNetwork = "yolov2_tiny_od_yolo_IR_fp32.xml";
    }
    if(!jsParser.parse("Classification.Model",clsNetwork)){
        std::cout<<"No classification model specified in config.json. Use default network path"<<std::endl;
        clsNetwork = "ResNet-50_fp32.xml";
    }
    if(!jsParser.parse("Decode.Video",videoFile)){
        std::cout<<"Error: No input video file specified in config.json"<<std::endl;
        return -1;
    }

    if(!checkValidNetFile(detNetwork) || !checkValidNetFile(clsNetwork) || !checkValidVideo(videoFile)){
        return -1;
    }

    // hva::hvaPipeline_t pl;

    // InferInputParams_t paramsInfer;  // input param for infer
    // paramsInfer.filenameModel = detNetwork;
    // //paramsInfer.filenameModel = "/opt/yolotiny/yolotiny.blob";
    // paramsInfer.format = INFER_FORMAT_NV12;
    // paramsInfer.postproc = InferNodeWorker::postprocessTinyYolov2WithClassify;
    // paramsInfer.preproc = InferNodeWorker::preprocessNV12;
    // auto& detectNode = pl.setSource(std::make_shared<InferNode>(1,1,STREAMS,paramsInfer), "DetectNode");

    // paramsInfer.filenameModel = clsNetwork;
    // //paramsInfer.filenameModel = "/opt/resnet/resnet.blob";
    // paramsInfer.format = INFER_FORMAT_NV12;
    // paramsInfer.postproc = InferNodeWorker::postprocessClassification;
    // paramsInfer.preproc = InferNodeWorker::preprocessNV12_ROI;
    // auto& classifyNode = pl.setSource(std::make_shared<InferNode>(1,0,STREAMS,paramsInfer), "ClassifyNode");

    // pl.linkNode("DetectNode", 0, "ClassifyNode", 0);

    // hva::hvaBatchingConfig_t config;
    // config.batchingPolicy = hva::hvaBatchingConfig_t::BatchingWithStream;
    // config.batchSize = 1;
    // config.streamNum = STREAMS;
    // config.threadNumPerBatch = 1;

    // detectNode.configBatch(config);
    // // classifyNode.configBatch(config);

    // pl.prepare();

    // pl.start();

    std::vector<std::thread*> vTh;
    vTh.reserve(STREAMS);
    std::promise<uint64_t> WIDPromise;
    std::future<uint64_t> WIDFuture = WIDPromise.get_future();

    using ms = std::chrono::milliseconds;

    hva::hvaPipeline_t pl;

    for(unsigned i = 0; i < STREAMS; ++i){
        // std::cout<<"starting thread "<<i<<std::endl;
        vTh.push_back(new std::thread([&, i](){
                    GstPipeContainer cont(i);
                    uint64_t WID = 0;
                    if(cont.init(videoFile, WID) != 0 || WID == 0){
                        std::cout<<"Fail to init cont!"<<std::endl;
                        return;
                    }

                    WIDPromise.set_value(WID);

                    std::this_thread::sleep_for(ms(10000));
                    std::cout<<"Dec source start feeding."<<std::endl;

                    std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
                    while(cont.read(blob)){
                        // pl.sendToPort(blob,"DetectNode",0,ms(0));
                        int* fd = blob->get<int, std::pair<unsigned,unsigned>>(0)->getPtr();
                        std::cout<<"FD received is "<<*fd<<std::endl;
                        blob.reset(new hva::hvaBlob_t());
                    }
                    // std::cout<<"Finished"<<std::endl;
                    return;
                }));
    }

    uint64_t WID = WIDFuture.get();
    auto& mynode2 = pl.setSource(std::make_shared<JpegEncNode>(1,0,1, WID), "JpegEncNode");

    pl.prepare();

    std::cout<<"\nPipeline Start: "<<std::endl;
    pl.start();

    for(unsigned i =0; i < STREAMS; ++i){
        vTh[i]->join();
    }

    std::cout<<"\nDec source joined "<<std::endl;

    std::this_thread::sleep_for(ms(5000));

    std::cout<<"Going to stop pipeline."<<std::endl;

    pl.stop();

    return 0;

}
