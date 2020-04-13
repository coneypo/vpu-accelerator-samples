#ifndef GST_PIPE_CONTAINER_HPP
#define GST_PIPE_CONTAINER_HPP

#include <gst/gst.h>
#include <string>
#include <hvaPipeline.hpp>
#include <cstdint>
#include <chrono>

inline float roundToHundredth(float x){
    return float(int(x*100.0+0.5))/100.0;
}

class GstPipeContainer{
public:
    struct Config{
        std::string filename;
        unsigned dropXFrame;
        unsigned dropEveryXFrame;
        bool enableFpsCounting;
        std::string codec;
    };

    GstPipeContainer(unsigned idx);

    int init(const Config& config, uint64_t& WID);

    int start();

    void stop();

    ~GstPipeContainer();

    bool read(std::shared_ptr<hva::hvaBlob_t>& blob);

    GstElement* pipeline;
    GstElement* file_source;
    GstElement* parser;
    GstElement* demux;
    GstElement* bypass;
    GstElement* dec;
    GstElement* tee;
    GstElement* vaapi_sink;
    GstElement* app_sink;
    GstElement* vaapi_queue;
    GstElement* app_queue;
    GstElement* capsfilter;

private:

    // class GstInitializer{
    //     GstInitializer(){
    //         gst_init(0, NULL);
    //     };
    // };

    // static GstInitializer gstInit;

    bool _gst_dmabuffer_import(GstBuffer *buffer, int& fd);
    bool validateConfig();
    int initH264Pipeline(uint64_t& WID);
    int initH264Pipeline(uint64_t& WID);
    int initContainerPipeline(uint64_t& WID);
    uint64_t queryWID();

    bool m_bStart;
    bool m_bStopped;

    GstPad* m_tee_vaapi_pad;
    GstPad* m_tee_app_pad;

    // GstSample* m_sampleRead;

    int m_width;
    int m_height;

    const unsigned m_idx;

    unsigned m_frameIdx; // exported frame index. Continuously increment
    unsigned m_frameCnt; // internal frame counter used for frame dropping, fps etc.

    uint64_t m_WID;

    Config m_config;

    std::chrono::time_point<std::chrono::high_resolution_clock> m_timeStart;
};

#endif //#ifndef GST_PIPE_CONTAINER_HPP