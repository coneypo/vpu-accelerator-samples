#include <GstPipeContainer.hpp>
#include <glib-object.h>
#include <gst/app/gstappsink.h>
#include <gst/allocators/allocators.h>
#include <gst/gststructure.h>
#include <gst/gstquery.h>
#include <va/va.h>
#include <iostream>
//#include <vpusmm/vpusmm.h>
#include <RemoteMemory.h>
#include <common.hpp>

GstPipeContainer::GstPipeContainer(unsigned idx):pipeline(nullptr), file_source(nullptr), 
            parser(nullptr), bypass(nullptr), dec(nullptr), app_sink(nullptr),m_bStart(false),
            demux(nullptr), capsfilter(nullptr), m_width(0), m_height(0), 
            m_idx(idx), m_frameIdx(0), m_frameCnt(0), m_bStopped(false){
}

int GstPipeContainer::init(const Config& config, uint64_t& WID){
    m_config = config;
    if(!validateConfig()){
        std::cout<<"Decoder config invalid!"<<std::endl;
        HVA_ERROR("Decoder config invalid");
        return -1;
    }

    if("h264"==m_config.codec){
        return initH264Pipeline(WID);
    }
    else if("h265"==m_config.codec){
        return initH265Pipeline(WID);
    }
    else if("mp4"==m_config.codec){
        return initContainerPipeline(WID);
    }
    else{
        return -1;
    }

}

int GstPipeContainer::initH264Pipeline(uint64_t& WID){

    file_source = gst_element_factory_make("multifilesrc", "file_source");
    parser = gst_element_factory_make("h264parse", "parser");
    bypass = gst_element_factory_make("bypass","bypass");
    dec = gst_element_factory_make("vaapih264dec", "dec");
    app_sink = gst_element_factory_make("appsink", "appsink");
    capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
    pipeline = gst_pipeline_new("pipeline");

    if(!file_source || !parser || !bypass || !dec || !app_sink || !pipeline
            || !capsfilter){
        HVA_ERROR("Fail to make one of the gstreamer plugins!");
        return -1;
    }

    GstCaps* caps = gst_caps_from_string("video/x-raw(memory:DMABuf), format=(string)NV12");
    if (!caps){
        HVA_ERROR("Fail to make gst caps");
        return -3;
    }
    g_object_set(capsfilter, "caps", caps, NULL);
    gst_caps_unref (caps);
    
    g_object_set(file_source, "location", m_config.filename.c_str(), NULL);
    g_object_set(file_source, "loop", true, NULL);

    gst_bin_add_many(GST_BIN(pipeline), file_source, parser, bypass, dec, capsfilter,
            app_sink, NULL);

    if(!gst_element_link_many(file_source, parser, bypass, dec, capsfilter, app_sink, NULL)){
        HVA_ERROR("Fail to link gst plugins");
        return -2;
    }

    WID = queryWID();
    if(WID == 0){
        HVA_ERROR("Failed to query WID");
        return -4;
    }

    return 0;
}

int GstPipeContainer::initH265Pipeline(uint64_t& WID){

    file_source = gst_element_factory_make("multifilesrc", "file_source");
    parser = gst_element_factory_make("h265parse", "parser");
    bypass = gst_element_factory_make("bypass","bypass");
    dec = gst_element_factory_make("vaapih265dec", "dec");
    app_sink = gst_element_factory_make("appsink", "appsink");
    capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
    pipeline = gst_pipeline_new("pipeline");

    if(!file_source || !parser || !bypass || !dec || !app_sink || !pipeline
            || !capsfilter){
        HVA_ERROR("Fail to make one of the gstreamer plugins!");
        return -1;
    }

    GstCaps* caps = gst_caps_from_string("video/x-raw(memory:DMABuf), format=(string)NV12");
    if (!caps){
        HVA_ERROR("Fail to make gst caps");
        return -3;
    }
    g_object_set(capsfilter, "caps", caps, NULL);
    gst_caps_unref (caps);
    
    g_object_set(file_source, "location", m_config.filename.c_str(), NULL);
    g_object_set(file_source, "loop", true, NULL);

    gst_bin_add_many(GST_BIN(pipeline), file_source, parser, bypass, dec, capsfilter,
            app_sink, NULL);

    if(!gst_element_link_many(file_source, parser, bypass, dec, capsfilter, app_sink, NULL)){
        HVA_ERROR("Fail to link gst plugins");
        return -2;
    }

    WID = queryWID();
    if(WID == 0){
        HVA_ERROR("Failed to query WID");
        return -4;
    }

    return 0;
}

static void on_demux_new_pad (GstElement *element, GstPad *pad, gpointer data)
{
    GstPad* parserSinkPad;
    GstElement *parser = (GstElement*)data;

    std::cout<<"Going to link Demux and Parser"<<std::endl;

    parserSinkPad = gst_element_get_static_pad(parser, "sink");
    gst_pad_link(pad, parserSinkPad);

    gst_object_unref(parserSinkPad);
}

int GstPipeContainer::initContainerPipeline(uint64_t& WID){

    file_source = gst_element_factory_make("filesrc", "file_source");
    demux = gst_element_factory_make("qtdemux", "demux");
    parser = gst_element_factory_make("h264parse", "parser");
    bypass = gst_element_factory_make("bypass","bypass");
    dec = gst_element_factory_make("vaapih264dec", "dec");
    app_sink = gst_element_factory_make("appsink", "appsink");
    capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
    pipeline = gst_pipeline_new("pipeline");

    if(!file_source || !demux || !parser || !bypass || !dec || !app_sink || !pipeline
            || !capsfilter){
        HVA_ERROR("Fail to make one of the gstreamer plugins!");
        return -1;
    }

    GstCaps* caps = gst_caps_from_string("video/x-raw(memory:DMABuf), format=(string)NV12");
    if (!caps){
        HVA_ERROR("Fail to make gst caps");
        return -3;
    }
    g_object_set(capsfilter, "caps", caps, NULL);
    gst_caps_unref (caps);
    
    g_object_set(file_source, "location", m_config.filename.c_str(), NULL);

    gst_bin_add_many(GST_BIN(pipeline), file_source, demux, parser, bypass, dec, capsfilter,
            app_sink, NULL);

    if(!gst_element_link_many(file_source, demux, NULL)){
        HVA_ERROR("Fail to link file src to demux");
        return -2;
    }
    if(!gst_element_link_many(parser, bypass, dec, capsfilter, app_sink, NULL)){
        HVA_ERROR("Fail to link gst plugins");
        return -2;
    }

    g_signal_connect(demux, "pad-added", G_CALLBACK (on_demux_new_pad), parser);

    WID = queryWID();
    if(WID == 0){
        HVA_ERROR("Failed to query WID");
        return -4;
    }

    return 0;
}

uint64_t GstPipeContainer::queryWID(){

    GstStructure* structure = gst_structure_new ("WIDQuery",
        "BypassQueryType", G_TYPE_STRING, "WorkloadContextQuery", NULL);
    GstQuery* query = gst_query_new_custom (GST_QUERY_CUSTOM, structure);

    if(!gst_element_query(bypass, query)){
        HVA_ERROR("Fail to query WID through gst_element_query");
        gst_query_unref(query);
        return 0;
    }

    structure = nullptr;

    uint64_t ret = 0;
    const GstStructure* WIDRet = gst_query_get_structure (query);

    if(!gst_structure_get_uint64(WIDRet,"WorkloadContextId", &ret)){
        HVA_ERROR("Fail to get Workload Contex ID from gst structure!");
        gst_query_unref(query);
        return 0;
    }

    m_WID = ret;

    HVA_DEBUG("WID Received: %u", m_WID);

    gst_query_unref(query);

    return ret;
}

GstPipeContainer::~GstPipeContainer(){
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

int GstPipeContainer::start(){
    m_height = 0;
    m_width = 0;
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if(m_config.enableFpsCounting){
        m_timeStart = std::chrono::high_resolution_clock::now();
    }
    m_bStart = true;
    return 0;
}

void GstPipeContainer::stop(){
    gst_element_set_state (pipeline, GST_STATE_NULL);
    m_bStopped = true;
}

bool GstPipeContainer::read(std::shared_ptr<hva::hvaBlob_t>& blob){
    if (!pipeline || !GST_IS_ELEMENT(pipeline)){
        std::cout<<"pipeline uninitialized! "<<std::endl;
        return false;
    }

    if(m_bStopped){
        HVA_INFO("Stop Gstreamer signal received");
        return false;
    }

    // start the pipeline if it was not in playing state yet
    if (!m_bStart)
        start();

    // bail out if EOS
    if (gst_app_sink_is_eos(GST_APP_SINK(app_sink))){
        HVA_INFO("EOS received");
        gst_element_seek_simple(pipeline, GST_FORMAT_TIME, (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), 0 * GST_SECOND);
        return read(blob);
    }

#ifdef VALIDATION_DUMP
    ms beforeRead = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now().time_since_epoch());
#endif
    GstSample* sampleRead = gst_app_sink_pull_sample(GST_APP_SINK(app_sink));
    if(!sampleRead){
        HVA_ERROR("Fail to read gstSample from appsink!");
        return false;
    }

    if(m_width == 0 || m_height == 0){
        GstCaps * frame_caps = gst_sample_get_caps(sampleRead);
        GstStructure* structure = gst_caps_get_structure(frame_caps, 0);
        gst_structure_get_int(structure, "width", &m_width);
        gst_structure_get_int(structure, "height", &m_height);
    }

    if(m_config.dropXFrame != 0 && m_frameCnt !=0 && m_frameCnt%m_config.dropEveryXFrame < m_config.dropXFrame){
        ++m_frameCnt;
        gst_sample_unref(sampleRead);
        sampleRead = nullptr;
        return read(blob);
    }
    else{
        // Does not Drop frame
        GstBuffer* buf = gst_sample_get_buffer(sampleRead);
        if (!buf){
            HVA_ERROR("Fail to retrieve buffer from gstSample");
            return false;
        }

        int fd = -1;
        GstMemory *mem = gst_buffer_get_memory(buf, 0);
        if(mem == nullptr){
            gst_memory_unref(mem);
            HVA_ERROR("Fail to get gstmemory from gstbuffer: nullptr!");
            return false;
        }

        unsigned long offset = 0;
        unsigned long maxSize = 0;
        unsigned long currentSize = gst_memory_get_sizes(mem, &offset, &maxSize);
        HVA_DEBUG("Current gstmem size: %lu, max size: %lu and offset: %lu", currentSize, maxSize, offset);

        if (!gst_is_dmabuf_memory(mem)) {
            gst_memory_unref(mem);
            HVA_ERROR("Fail to get dmabuf from gstmem: not dmabuf!");
            return false;
        } 
        else {
            fd = gst_dmabuf_memory_get_fd(mem);
            if (fd <= 0) {
                gst_memory_unref(mem);
                HVA_ERROR("Fail to get fd from gstmemory!");
                return false;
            }

            HVA_DEBUG("FD from dmabuf is %ul with frame id %u", fd, m_frameIdx);
        }

        blob->streamId = m_idx;
        blob->frameId = m_frameIdx;

        float decFps = 0.0;
        if(m_config.enableFpsCounting){
            std::chrono::duration<double, std::milli> elapsedMs = std::chrono::high_resolution_clock::now() - m_timeStart;
            decFps = roundToHundredth((m_frameCnt+1)/elapsedMs.count()*1000.0);
        }

        blob->emplace<int, VideoMeta>(new int(fd), m_width*m_height*3/2,
                new VideoMeta{(unsigned)m_width, (unsigned)m_height, currentSize, decFps
#ifdef VALIDATION_DUMP
                , beforeRead, ms(0)
#endif
                , false
                },[mem, sampleRead](int* fd, VideoMeta* meta){
                    HVA_DEBUG("Preparing to destruct fd %u", *fd);
                    gst_memory_unref(mem);
                    gst_sample_unref(sampleRead);
                    delete fd;
                    delete meta;
                });

        ++m_frameIdx;
        ++m_frameCnt;

        return true;
    }

}

bool GstPipeContainer::validateConfig(){
    if(m_config.filename.empty()){
        std::cout<<"Filename empty!"<<std::endl;
        HVA_ERROR("Video filename empty!");
        return false;
    }

    if("h264"!=m_config.codec && "h265"!=m_config.codec && "mp4"!=m_config.codec ){
        std::cout<<"Unsupported codec format"<<std::endl;
        HVA_ERROR("Unsupported codec format from decoder");
        return false;
    }

    if(m_config.dropXFrame == 0){
        m_config.dropEveryXFrame = 1024;
    }
    else{
        if(m_config.dropEveryXFrame > 1024){
            m_config.dropEveryXFrame = 1024;
        }
        else{
            if(m_config.dropEveryXFrame <= 1){
                m_config.dropEveryXFrame = 2;
            }
        }
        
        if(m_config.dropXFrame >= m_config.dropEveryXFrame){
            m_config.dropXFrame = m_config.dropEveryXFrame - 1;
        }
    }
    return true;
}

/**
bool GstPipeContainer::_gst_dmabuffer_import(GstBuffer *buffer, int& fd){
    GstMemory *mem = gst_buffer_get_memory(buffer, 0);
    if (mem == nullptr || !gst_is_dmabuf_memory(mem)) {
        gst_memory_unref(mem);
        return false;
    } else {
        fd = gst_dmabuf_memory_get_fd(mem);
        if (fd <= 0) {
            gst_memory_unref(mem);
            return false;
        } else {
            long int phyAddr = vpusmm_import_dmabuf(fd, VPU_DEFAULT);
            if (phyAddr <= 0) {
                gst_memory_unref(mem);
                return false;
            }
        }
    }
    gst_memory_unref(mem);
    return true;

}
**/
