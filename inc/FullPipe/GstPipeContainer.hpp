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

// the gstreamer pipeline container. Note that due to vaapi restriction on thread and workload context
//  creation, the pipeline construction/initialization must be called within a 'virgin' thread that has
//  no other vaapi session ran before
class GstPipeContainer{
public:
    struct Config{
        std::string filename;       // file name of video source
        unsigned dropXFrame;        // obsoleted
        unsigned dropEveryXFrame;   // obsoleted
        bool enableFpsCounting;     // enable fps profiling and record in meta
        std::string codec;          // h264, h265 or mp4. Note that h265 only supports 8bit data length
    };

    /**
    * @brief construct a gstreamer pipeline container
    * 
    * @param idx index assigned to this gstreamer pipeline container. This index will be set to the output
    *           stream id
    * @return void
    * 
    */  
    GstPipeContainer(unsigned idx);

    /**
    * @brief initialize the gstreamer pipeline container and validate its configuration
    * 
    * @param config decoder configurations
    * @param WID workload context id as an output. This WID is used to passed to jpeg encoder so that they
    *           share the same WID
    * @return return 0 if success
    * 
    */
    int init(const Config& config, uint64_t& WID);

    /**
    * @brief start the decoder
    * 
    * @param void
    * @return void
    * 
    */
    int start();

    /**
    * @brief stop the decoder and destruct relevant resources
    * 
    * @param void
    * @return void
    * 
    */
    void stop();

    ~GstPipeContainer();

    /**
    * @brief read a decoded frame from decoder. Currently the decoded frame is within format of KMB 
    *           remote FD
    * 
    * @param blob decoded frame wrapped in hvaBlob_t, which will be sent to the hva pipeline
    * @return read status
    * 
    */
    bool read(std::shared_ptr<hva::hvaBlob_t>& blob);

    GstElement* pipeline;
    GstElement* file_source;
    GstElement* parser;
    GstElement* demux;
    GstElement* bypass;
    GstElement* dec;
    GstElement* app_sink;
    GstElement* capsfilter;

private:
    // bool _gst_dmabuffer_import(GstBuffer *buffer, int& fd);
    bool validateConfig();
    int initH264Pipeline(uint64_t& WID);
    int initH265Pipeline(uint64_t& WID);
    int initContainerPipeline(uint64_t& WID);
    uint64_t queryWID();

    bool m_bStart;
    bool m_bStopped;
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