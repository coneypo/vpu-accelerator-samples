#ifndef GST_HVA_SAMPLE_HPP
#define GST_HVA_SAMPLE_HPP

#include <gst/gst.h>

class GstPipeContainer{
public:
    GstPipeContainer():pipeline(nullptr), file_source(nullptr), tee(nullptr),
            parser(nullptr), dec(nullptr), vaapi_sink(nullptr), app_sink(nullptr){

    };

    GstElement* pipeline;
    GstElement* file_source;
    GstElement* parser;
    GstElement* dec;
    GstElement* tee;
    GstElement* vaapi_sink;
    GstElement* app_sink;
};

#endif //#ifndef GST_HVA_SAMPLE_HPP