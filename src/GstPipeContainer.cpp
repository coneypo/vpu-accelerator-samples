#include <GstPipeContainer.hpp>
#include <glib-object.h>
#include <gst/app/gstappsink.h>
#include <iostream>
#include <vpusmm/vpusmm.h>

GstPipeContainer::GstPipeContainer():pipeline(nullptr), file_source(nullptr), tee(nullptr),
            parser(nullptr), dec(nullptr), vaapi_sink(nullptr), app_sink(nullptr),m_bStart(false),
            m_tee_vaapi_pad(nullptr), m_tee_app_pad(nullptr), vaapi_queue(nullptr), app_queue(nullptr),
            m_width(0), m_height(0){

}

int GstPipeContainer::init(){
    file_source = gst_element_factory_make("filesrc", "file_source");
    parser = gst_element_factory_make("h264parse", "parser");
    dec = gst_element_factory_make("vaapih264dec", "dec");
    tee = gst_element_factory_make("tee", "tee");
    vaapi_queue = gst_element_factory_make("queue", "vaapi_queue");
    app_queue = gst_element_factory_make("queue", "app_queue");
    vaapi_sink = gst_element_factory_make("vaapisink", "vaapi_sink");
    app_sink = gst_element_factory_make("appsink", "appsink");

    pipeline = gst_pipeline_new("pipeline");

    if(!file_source || !parser || !dec || !tee || !vaapi_sink ||
            !app_sink || !vaapi_queue || !app_queue || !pipeline){
        return -1;
    }
    // if(!cont.file_source || !cont.parser || !cont.dec || !cont.vaapi_sink ||
    //         !cont.pipeline){
    //     std::cout<<"Element init failed!"<<std::endl;
    //     std::cout<<"Element cont.file_source"<< cont.file_source<<std::endl;
    //     std::cout<<"Element cont.parser"<< cont.parser<<std::endl;
    //     std::cout<<"Element cont.dec"<< cont.dec<<std::endl;
    //     std::cout<<"Element cont.vaapi_sink"<< cont.vaapi_sink<<std::endl;
    //     return -1;
    // }

    // video/x-raw(memory:VASurface)
    GstCaps *caps = gst_caps_new_simple("video/x-raw", 
            "format", G_TYPE_STRING, "NV12", NULL);
    g_object_set(app_sink, "caps", caps, NULL);
    gst_caps_unref(caps);

    g_object_set(file_source, "location", "/Workspace/barrier_1080x720.h264", NULL);

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

    // gst_bin_add_many(GST_BIN(cont.pipeline), cont.file_source, cont.parser, cont.dec,
    //         cont.vaapi_sink, NULL);

    // if(!gst_element_link_many(cont.file_source, cont.parser, cont.dec, cont.vaapi_sink, NULL)){
    //     return -1;
    // }

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

    return 0;
}

GstPipeContainer::~GstPipeContainer(){
    gst_element_release_request_pad(tee, m_tee_vaapi_pad);
    gst_element_release_request_pad(tee, m_tee_app_pad);
    if(!m_tee_vaapi_pad)
        gst_object_unref(m_tee_vaapi_pad);
    if(!m_tee_app_pad)
        gst_object_unref(m_tee_app_pad);
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

int GstPipeContainer::start(){
    m_height = 0;
    m_width = 0;
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    m_bStart = true;
}

bool GstPipeContainer::read(std::shared_ptr<hva::hvaBlob_t>& blob){
    if (!pipeline || !GST_IS_ELEMENT(pipeline))
        return false;

    // start the pipeline if it was not in playing state yet
    if (!m_bStart)
        start();

    // bail out if EOS
    if (gst_app_sink_is_eos(GST_APP_SINK(app_sink)))
        return false;

    // if(!m_sampleRead){
    //     gst_sample_unref(m_sampleRead);
    //     m_sampleRead = nullptr;
    // }
    GstSample* sampleRead = gst_app_sink_pull_sample(GST_APP_SINK(app_sink));
    if(!sampleRead)
        return false;

    if(m_width == 0 || m_height == 0){
        GstCaps * frame_caps = gst_sample_get_caps(sampleRead);
        GstStructure* structure = gst_caps_get_structure(frame_caps, 0);
        gst_structure_get_int(structure, "width", &m_width);
        gst_structure_get_int(structure, "height", &m_height);
    }

    GstBuffer* buf = gst_sample_get_buffer(sampleRead);
    if (!buf)
        return false;

    // GstMapInfo info = {};
    GstMapInfo* info = new GstMapInfo;
    if (!gst_buffer_map(buf, info, GST_MAP_READ))
        return false;

    

    blob->emplace<unsigned char, std::pair<unsigned, unsigned>>(info->data, m_width*m_height*3/2,
            new std::pair<unsigned, unsigned>(m_width,m_height),[buf, info, sampleRead](unsigned char* psuf, std::pair<unsigned, unsigned>* meta){
                gst_buffer_unmap(buf, info);
                gst_sample_unref(sampleRead);
                delete info;
                delete meta;
            });

    return true;

/*
    uint8_t* cur = info.data;
    std::cout<<"\nFrame Received: "<<std::endl;
    for(unsigned row = 0; row < m_height; ++row){
        for(unsigned col = 0; col < m_width; ++col){
            std::cout<< std::hex << (unsigned)*cur << " ";
            ++cur;
        }
        std::cout<< std::dec <<std::endl;
    }

    gst_buffer_unmap(buf, &info);
    gst_sample_unref(m_sampleRead);
    m_sampleRead = nullptr;
*/
}

bool GstPipeContainer::_gst_dmabuffer_import(GstBuffer *buffer){
    GstMemory *mem = gst_buffer_get_memory(buffer, 0);
    if (mem == nullptr || !gst_is_dmabuf_memory(mem)) {
        gst_memory_unref(mem);
        return false;
    } else {
        int fd = gst_dmabuf_memory_get_fd(mem);
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