#ifndef GST_PIPE_CONTAINER_HPP
#define GST_PIPE_CONTAINER_HPP

#include <gst/gst.h>

class GstPipeContainer{
public:
    GstPipeContainer();

    int init();

    int start();

    ~GstPipeContainer();

    bool read();

    GstElement* pipeline;
    GstElement* file_source;
    GstElement* parser;
    GstElement* dec;
    GstElement* tee;
    GstElement* vaapi_sink;
    GstElement* app_sink;
    GstElement* vaapi_queue;
    GstElement* app_queue;

private:
    bool m_bStart;

    GstPad* m_tee_vaapi_pad;
    GstPad* m_tee_app_pad;

    GstSample* m_sampleRead;

    int m_width;
    int m_height;
};

#endif //#ifndef GST_PIPE_CONTAINER_HPP