//
// Created by xiao on 2020/2/19.
//

#ifndef HDDLDEMO_PIPELINE_H
#define HDDLDEMO_PIPELINE_H

#include "utils/messagetype.h"

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/videooverlay.h>
#include <memory>

class FpsStat;

class Pipeline {
public:
    Pipeline() = default;
    virtual ~Pipeline();
    bool parse(const char* pipeline, const char* displaySink);
    bool run(guintptr winhandler, int timeout = 0);
    void process(PipelineAction action);
    void exposeVideoOverlay();
    double getFps();

private:
    static gboolean busCallBack(GstBus* bus, GstMessage* msg, gpointer data);

    GstElement* m_pipeline { nullptr };
    GstElement* m_displaySink { nullptr };
    GstVideoOverlay* m_overlay { nullptr };
    std::shared_ptr<FpsStat> m_fpsProb { nullptr };
};

#endif //HDDLDEMO_PIPELINE_H
