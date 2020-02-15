//
// Created by xiao on 2020/1/22.
//

#include "Pipeline.h"

Pipeline::~Pipeline()
{
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        g_object_unref(m_pipeline);
    }
}

bool Pipeline::parse(const std::string& pipelineConfig)
{
    m_pipeline = gst_parse_launch(pipelineConfig.c_str(), NULL);
    if (!m_pipeline) {
        g_printerr("Pipeline Parse Error: %s", pipelineConfig.c_str());
        return false;
    }
    return true;
}

GstElement* Pipeline::getElementByName(const std::string& name)
{
    GstElement* element = gst_bin_get_by_name(GST_BIN(m_pipeline), name.c_str());
    if (!element) {
        g_printerr("Can't get element by name : %s", name.c_str());
    }
    return element;
}

GstPad* Pipeline::getPadFromElement(const std::string& elementName, const std::string& pad)
{
    auto element = getElementByName(elementName);
    return gst_element_get_static_pad(element, pad.c_str());
}

template <typename T>
void Pipeline::setElementProperty(const std::string& elementName, const std::string& propertyName, T t)
{
    auto element = getElementByName(elementName);
    if (element) {
        g_object_set(element, propertyName.c_str(), t, NULL);
    }
}

void Pipeline::setElementSignalCallBack(const std::string& elementName, const std::string& signalName, GCallback callback, gpointer data)
{
    auto element = getElementByName(elementName);
    if (element) {
        g_signal_connect(element, signalName.c_str(), G_CALLBACK(callback), data);
    }
}

void Pipeline::setBusCallBack(GstBusFunc func, gpointer data)
{
    GstBus* bus = gst_element_get_bus(m_pipeline);
    // TODO release bus watch id
    guint busWatchId = gst_bus_add_watch(bus, func, data);
    gst_object_unref(bus);
}

GstVideoOverlay* Pipeline::getVideoOverlay(const std::string& elementName, const std::string& overlayName)
{
    auto element = getElementByName(elementName);
    if (GST_IS_VIDEO_OVERLAY(element)) {
        return GST_VIDEO_OVERLAY(element);
    } else if (!overlayName.empty()) {
        return GST_VIDEO_OVERLAY(getElementByName(overlayName));
    }

    return nullptr;
}

bool Pipeline::run()
{
    GstStateChangeReturn sret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    return !(sret == GST_STATE_CHANGE_FAILURE);
}
