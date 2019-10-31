#include <GstHvaSample.hpp>
#include <glib-object.h>
#include <iostream>

int main(){

    gst_init(0, NULL);

    GstPipeContainer cont;

    cont.file_source = gst_element_factory_make("filesrc", "file_source");
    cont.parser = gst_element_factory_make("h264parse", "parser");
    cont.dec = gst_element_factory_make("vaapih264dec", "dec");
    cont.tee = gst_element_factory_make("tee", "tee");
    cont.vaapi_sink = gst_element_factory_make("vaapisink", "vaapi_sink");
    cont.app_sink = gst_element_factory_make("appsink", "appsink");

    cont.pipeline = gst_pipeline_new("pipeline");

    if(!cont.file_source || !cont.parser || !cont.dec || !cont.tee || !cont.vaapi_sink ||
            !cont.app_sink || !cont.pipeline){
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

    GstCaps *caps = gst_caps_new_simple("video/x-raw(memory:VASurface)", 
            "format", G_TYPE_STRING, "NV12", NULL);
    g_object_set(cont.app_sink, "caps", caps, NULL);
    gst_caps_unref(caps);

    g_object_set(cont.file_source, "location", "/Workspace/barrier_1080x720.h264", NULL);

    gst_bin_add_many(GST_BIN(cont.pipeline), cont.file_source, cont.parser, cont.dec,
            cont.tee, cont.vaapi_sink, cont.app_sink, NULL);
    if(!gst_element_link_many(cont.file_source, cont.parser, cont.dec, cont.tee, NULL)){
        return -1;
    }

    // gst_bin_add_many(GST_BIN(cont.pipeline), cont.file_source, cont.parser, cont.dec,
    //         cont.vaapi_sink, NULL);

    // if(!gst_element_link_many(cont.file_source, cont.parser, cont.dec, cont.vaapi_sink, NULL)){
    //     return -1;
    // }

    GstPad* tee_vaapi_pad = gst_element_get_request_pad(cont.tee, "src_%u");
    GstPad* tee_app_pad = gst_element_get_request_pad(cont.tee, "src_%u");

    GstPad* vaapi_pad = gst_element_get_static_pad(cont.vaapi_sink, "sink");
    GstPad* app_pad = gst_element_get_static_pad(cont.app_sink, "sink");

    if(gst_pad_link(tee_vaapi_pad, vaapi_pad) != GST_PAD_LINK_OK ||
            gst_pad_link(tee_app_pad, app_pad) != GST_PAD_LINK_OK){
        return -1;
    }

    gst_object_unref(vaapi_pad);
    gst_object_unref(app_pad);

    gst_element_set_state(cont.pipeline, GST_STATE_PLAYING);

    /* Wait until error or EOS */
    GstBus* bus = gst_element_get_bus(cont.pipeline);
    GstMessage* msg =
        gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
        (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    /* Free resources */
    if (msg != NULL)
        gst_message_unref (msg);
    gst_object_unref (bus);
    gst_element_release_request_pad(cont.tee, tee_vaapi_pad);
    gst_element_release_request_pad(cont.tee, tee_app_pad);
    gst_object_unref(tee_vaapi_pad);
    gst_object_unref(tee_app_pad);
    gst_element_set_state (cont.pipeline, GST_STATE_NULL);
    gst_object_unref(cont.pipeline);
    return 0;

    // GstElement *pipeline;
    // GstBus *bus;
    // GstMessage *msg;

    // /* Initialize GStreamer */
    // gst_init(0, NULL);

    // /* Build the pipeline */
    // pipeline =
    //     gst_parse_launch
    //     ("filesrc location=/Workspace/barrier_1080x720.h264 ! h264parse ! vaapih264dec ! vaapisink",
    //     NULL);

    // /* Start playing */
    // gst_element_set_state (pipeline, GST_STATE_PLAYING);

    // /* Wait until error or EOS */
    // bus = gst_element_get_bus (pipeline);
    // msg =
    //     gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
    //     (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    // /* Free resources */
    // if (msg != NULL)
    //     gst_message_unref (msg);
    // gst_object_unref (bus);
    // gst_element_set_state (pipeline, GST_STATE_NULL);
    // gst_object_unref (pipeline);
    // return 0;
}