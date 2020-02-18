#include <GstPipeContainer.hpp>
#include <glib-object.h>
#include <gst/app/gstappsink.h>
#include <gst/allocators/allocators.h>
#include <va/va.h>
#include <iostream>
//#include <vpusmm/vpusmm.h>

GstPipeContainer::GstPipeContainer(unsigned idx):pipeline(nullptr), file_source(nullptr), tee(nullptr),
            parser(nullptr), dec(nullptr), vaapi_sink(nullptr), app_sink(nullptr),m_bStart(false),
            m_tee_vaapi_pad(nullptr), m_tee_app_pad(nullptr), vaapi_queue(nullptr), app_queue(nullptr),
            capsfilter(nullptr), m_width(0), m_height(0), m_idx(idx), m_frameIdx(0){
}

int GstPipeContainer::init(std::string filename){
    file_source = gst_element_factory_make("filesrc", "file_source");
    parser = gst_element_factory_make("h264parse", "parser");
    dec = gst_element_factory_make("vaapih264dec", "dec");
    app_queue = gst_element_factory_make("queue", "app_queue");
    app_sink = gst_element_factory_make("appsink", "appsink");
#ifdef ENABLE_DISPLAY
    tee = gst_element_factory_make("tee", "tee");
    vaapi_queue = gst_element_factory_make("queue", "vaapi_queue");
    vaapi_sink = gst_element_factory_make("vaapisink", "vaapi_sink");
#else
    capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
#endif
    pipeline = gst_pipeline_new("pipeline");

    if(!file_source || !parser || !dec || !app_sink || !app_queue || !pipeline
#ifdef ENBALE_DISPLAY
            || !vaapi_queue || !vaapi_sink || !tee
#else
            || !capsfilter
#endif
            ){
        return -1;
    }

/*
#ifdef ENABLE_DISPLAY
    GstCaps *caps = gst_caps_new_simple("video/x-raw", 
            "format", G_TYPE_STRING, "NV12", NULL);
#else
    GstCaps *caps = gst_caps_new_simple("video/x-raw(memory:DMABuf)", 
            "format", G_TYPE_STRING, "NV12", NULL);
#endif
    g_object_set(app_sink, "caps", caps, NULL);
    gst_caps_unref(caps);
*/

    // GstCaps* caps = gst_caps_from_string("video/x-raw, format=(string)NV12");
    GstCaps* caps = gst_caps_from_string("video/x-raw(memory:DMABuf), format=(string)NV12");
    if (!caps)
        return -3;
    g_object_set(capsfilter, "caps", caps, NULL);
    gst_caps_unref (caps);
    
    // g_object_set(file_source, "location", "./barrier_1080x720.h264", NULL);
    g_object_set(file_source, "location", filename.c_str(), NULL);

#ifdef ENABLE_DISPLAY
    gst_bin_add_many(GST_BIN(pipeline), file_source, parser, dec,
            tee, vaapi_queue, app_queue, vaapi_sink, app_sink, NULL);
    if(!gst_element_link_many(file_source, parser, dec, tee, NULL)){
        return -2;
    }

    if(!gst_element_link_many(vaapi_queue, vaapi_sink, NULL)){
        return -2;
    }
    if(!gst_element_link_many(app_queue, app_sink, NULL)){
        return -2;
    }
#else
    gst_bin_add_many(GST_BIN(pipeline), file_source, parser, dec, capsfilter,
            app_queue, app_sink, NULL);

    if(!gst_element_link_many(file_source, parser, dec, capsfilter, app_queue, app_sink, NULL)){
        return -2;
    }
#endif

#ifdef ENABLE_DISPLAY
    m_tee_vaapi_pad = gst_element_get_request_pad(tee, "src_%u");
    m_tee_app_pad = gst_element_get_request_pad(tee, "src_%u");

    GstPad* vaapi_pad = gst_element_get_static_pad(vaapi_queue, "sink");
    GstPad* app_pad = gst_element_get_static_pad(app_queue, "sink");

    if(gst_pad_link(m_tee_vaapi_pad, vaapi_pad) != GST_PAD_LINK_OK ||
            gst_pad_link(m_tee_app_pad, app_pad) != GST_PAD_LINK_OK){
        return -3;
    }


    gst_object_unref(vaapi_pad);
    gst_object_unref(app_pad);
#endif

    return 0;
}

GstPipeContainer::~GstPipeContainer(){
#ifdef ENABLE_DISPLAY
    gst_element_release_request_pad(tee, m_tee_vaapi_pad);
    gst_element_release_request_pad(tee, m_tee_app_pad);
    if(!m_tee_vaapi_pad)
        gst_object_unref(m_tee_vaapi_pad);
    if(!m_tee_app_pad)
        gst_object_unref(m_tee_app_pad);
#endif
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

int GstPipeContainer::start(){
    m_height = 0;
    m_width = 0;
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    m_bStart = true;
    return 0;
}

bool GstPipeContainer::read(std::shared_ptr<hva::hvaBlob_t>& blob){
    if (!pipeline || !GST_IS_ELEMENT(pipeline)){
        std::cout<<"pipeline uninitialized! "<<std::endl;
        return false;
    }
    // start the pipeline if it was not in playing state yet
    if (!m_bStart)
        start();

    // bail out if EOS
    if (gst_app_sink_is_eos(GST_APP_SINK(app_sink))){
        std::cout<<"EOS reached"<<std::endl;
        return false;
    }

    // if(!m_sampleRead){
    //     gst_sample_unref(m_sampleRead);
    //     m_sampleRead = nullptr;
    // }
    GstSample* sampleRead = gst_app_sink_pull_sample(GST_APP_SINK(app_sink));
    if(!sampleRead){
        std::cout<<"Read sample failed!"<<std::endl;
        return false;
    }

    if(m_width == 0 || m_height == 0){
        GstCaps * frame_caps = gst_sample_get_caps(sampleRead);
        GstStructure* structure = gst_caps_get_structure(frame_caps, 0);
        gst_structure_get_int(structure, "width", &m_width);
        gst_structure_get_int(structure, "height", &m_height);
    }

    GstBuffer* buf = gst_sample_get_buffer(sampleRead);
    if (!buf){
        std::cout<<"Retrieve buffer failed!"<<std::endl;
        return false;
    }

    // GstMapInfo info = {};
    GstMapInfo* info = new GstMapInfo;
    if (!gst_buffer_map(buf, info, GST_MAP_READ)){
        std::cout<<"Buffer map failed!"<<std::endl;
        return false;
    }

    // int fd = -1;
    // if (!_gst_dmabuffer_import(buf, fd)){
    //     std::cout<<"DMA buffer import failed!"<<std::endl;
    //     return false;
    // }

    blob->streamId = m_idx;
    blob->frameId = m_frameIdx;
    //std::cout<<"!!!!!!Decoder set frame id to "<<blob->frameId<<std::endl;

    //std::cout<<"Stream "<<blob->streamId<<" frame "<<blob->frameId<<" pushed"<<std::endl;

    // blob->emplace<unsigned char, std::pair<unsigned, unsigned>>(info->data, m_width*m_height*3/2,
    //         new std::pair<unsigned, unsigned>(m_width,m_height),[buf, info, sampleRead, fd](unsigned char* psuf, std::pair<unsigned, unsigned>* meta){
    //             vpusmm_unimport_dmabuf(fd);
    //             gst_buffer_unmap(buf, info);
    //             gst_sample_unref(sampleRead);
    //             delete info;
    //             delete meta;
    //         });

    blob->emplace<unsigned char, std::pair<unsigned, unsigned>>(info->data, m_width*m_height*3/2,
            new std::pair<unsigned, unsigned>(m_width,m_height),[buf, info, sampleRead](unsigned char* psuf, std::pair<unsigned, unsigned>* meta){
                gst_buffer_unmap(buf, info);
                gst_sample_unref(sampleRead);
                delete info;
                delete meta;
            });

    ++m_frameIdx;

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
