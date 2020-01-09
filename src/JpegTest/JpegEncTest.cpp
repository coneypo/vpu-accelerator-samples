#include <hvaPipeline.hpp>
#include <jpeg_enc_node.hpp>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>

using ms = std::chrono::milliseconds;


struct InfoROI_t {
    int widthImage = 0;
    int heightImage = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int indexROI = 0;
    int totalROINum = 0;
    int frameId = 0;
};

class TestFeederWorker : public hva::hvaNodeWorker_t{
public:

    TestFeederWorker(hva::hvaNode_t* parentNode):hva::hvaNodeWorker_t(parentNode){ };

    virtual void process(std::size_t batchIdx) override{
        InfoROI_t* pInfoROI = new InfoROI_t();
        pInfoROI->height = 30;
        pInfoROI->width = 30;
        pInfoROI->widthImage = 1080;
        pInfoROI->heightImage = 720;
        pInfoROI->x = 66;
        pInfoROI->y = 99;
        pInfoROI->indexROI = 5;
        pInfoROI->totalROINum = 13;
        pInfoROI->frameId = 2;

        std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());
        blob->emplace<unsigned char, InfoROI_t>(nullptr, 0u,
                pInfoROI, [](unsigned char* mem, InfoROI_t* meta){
                    // if (nullptr != mem) {
                    //     delete[] mem;
                    // } 
                    delete meta;
                });

        blob->emplace<unsigned char, std::pair<unsigned, unsigned>>((unsigned char*)m_buf, 1080*720*3/2,
            new std::pair<unsigned, unsigned>(1080,720),[](unsigned char* psuf, std::pair<unsigned, unsigned>* meta){
                delete meta;
            });

        sendOutput(blob,0);
        std::cout<<"Blob sent"<<std::endl;

    };

    virtual void init() override{
        m_in.open("/home/kezhen/Workspace/out.yuv", std::ios::in|std::ios::binary|std::ios::ate);
        if(m_in.is_open()){
            m_size = m_in.tellg();
            m_buf = new char[m_size];
            m_in.seekg(0, std::ios::beg);
            m_in.read(m_buf, m_size);
            m_in.close();
            std::cout<<"Input read into the buffer."<<std::endl;
        }
        else{
            std::cout<<"Fail to open input file!"<<std::endl;
        }
    };

    virtual void deinit() override{
        delete[] m_buf;
    };

private:
    std::ifstream m_in;
    unsigned m_size;
    char* m_buf;
};

class TestFeeder : public hva::hvaNode_t{
public:
    TestFeeder(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum)
            :hva::hvaNode_t(inPortNum, outPortNum, totalThreadNum){

    };

    ~TestFeeder(){

    };

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override{
        return std::shared_ptr<hva::hvaNodeWorker_t>(new TestFeederWorker((hva::hvaNode_t*)this));
    };

private:

};

int main(){
    std::cout<<"Test Start:"<<std::endl;

    hva::hvaPipeline_t pl;

    auto& mynode1 = pl.setSource(std::make_shared<TestFeeder>(0,1,1), "TestFeeder");

    auto& mynode2 = pl.addNode(std::make_shared<JpegEncNode>(1,0,1), "JpegEncNode");

    pl.linkNode("TestFeeder", 0, "JpegEncNode", 0);

    mynode1.configLoopingInterval(ms(5000));

    pl.prepare();

    std::cout<<"\nPipeline Start: "<<std::endl;
    pl.start();

    std::this_thread::sleep_for(ms(10000));

    std::cout<<"Going to stop pipeline."<<std::endl;

    pl.stop();
}
