#include <GstHvaSample.hpp>
#include <iostream>
#include <memory>
#include <hvaPipeline.hpp>
#include <infer_node.hpp>
#include <chrono>
#include <thread>

#define STREAMS 4

int main(){

    gst_init(0, NULL);

    hva::hvaPipeline_t pl;

    InferInputParams_t paramsInfer;  // input param for infer
    // paramsInfer.filenameModel = "yolov2_tiny_od_yolo_IR_fp32.xml";
    paramsInfer.filenameModel = "/opt/yolotiny/yolotiny.blob";
    paramsInfer.format = INFER_FORMAT_NV12;
    paramsInfer.postproc = InferNodeWorker::postprocessTinyYolov2WithClassify;
    paramsInfer.preproc = InferNodeWorker::preprocessNV12;
    auto& detectNode = pl.setSource(std::make_shared<InferNode>(1,1,STREAMS,paramsInfer), "DetectNode");

    paramsInfer.filenameModel = "/opt/resnet/resnet.blob";
    paramsInfer.format = INFER_FORMAT_NV12;
    paramsInfer.postproc = InferNodeWorker::postprocessClassification;
    paramsInfer.preproc = InferNodeWorker::preprocessNV12_ROI;
    auto& classifyNode = pl.setSource(std::make_shared<InferNode>(1,0,STREAMS,paramsInfer), "ClassifyNode");

    pl.linkNode("DetectNode", 0, "ClassifyNode", 0);

    hva::hvaBatchingConfig_t config;
    config.batchingPolicy = hva::hvaBatchingConfig_t::BatchingWithStream;
    config.batchSize = 1;
    config.streamNum = STREAMS;
    config.threadNumPerBatch = 1;

    detectNode.configBatch(config);
    classifyNode.configBatch(config);

    pl.prepare();

    pl.start();

    // GstPipeContainer cont;
    // cont.init();
    // cont.start();

    std::vector<std::thread*> vTh;
    vTh.reserve(STREAMS);

    using ms = std::chrono::milliseconds;

    for(unsigned i = 0; i < STREAMS; ++i){
        std::cout<<"starting thread "<<i<<std::endl;
        vTh.push_back(new std::thread([&, i](){
                    GstPipeContainer cont(i);
                    cont.init();

                    std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
                    while(cont.read(blob)){
                        pl.sendToPort(blob,"DetectNode",0,ms(0));
                        blob.reset(new hva::hvaBlob_t());
                    }
                    std::cout<<"Finished"<<std::endl;
                }));
    }

    // std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
    // while(cont.read(blob)){
    //     pl.sendToPort(blob,"DetectNode",0);
    //     blob.reset(new hva::hvaBlob_t());
    // }
    // std::cout<<"Finished"<<std::endl;

    for(unsigned i =0; i < STREAMS; ++i){
        vTh[i]->join();
    }

    std::this_thread::sleep_for(ms(20000));

    std::cout<<"Going to stop pipeline."<<std::endl;

    pl.stop();

    // /* Wait until error or EOS */
    // GstBus* bus = gst_element_get_bus(cont.pipeline);
    // GstMessage* msg =
    //     gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
    //     (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    // /* Free resources */
    // if (msg != NULL)
    //     gst_message_unref (msg);
    // gst_object_unref (bus);

    return 0;

}