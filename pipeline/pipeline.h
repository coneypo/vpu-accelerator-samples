/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */


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
    void setSize(guint32 width, guint32 height);
    void exposeVideoOverlay();
    double getFps();
    double getOffloadPipeDecodingFps();
    double getOffloadPipeInferenceFps();

private:
    static gboolean busCallBack(GstBus* bus, GstMessage* msg, gpointer data);
    void getVideoResolution(const std::string& mediaFile);
    //only used for streaming mode
    void configureRoiSink(const std::string& pipeline);

    GstElement* m_pipeline { nullptr };
    GstElement* m_displaySink { nullptr };
    GstVideoOverlay* m_overlay { nullptr };
    std::shared_ptr<FpsStat> m_fpsProb { nullptr };
    guint32 m_videoWidth { 0 };
    guint32 m_videoHeight { 0 };
};

#endif //HDDLDEMO_PIPELINE_H
