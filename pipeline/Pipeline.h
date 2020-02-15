//
// Created by xiao on 2020/1/22.
//

#ifndef HDDLDEMO_PIPELINEPARSER_H
#define HDDLDEMO_PIPELINEPARSER_H

#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <stdarg.h>
#include <string>

class Pipeline {
public:
    Pipeline() = default;
    virtual ~Pipeline();

    bool parse(const std::string& pipelineConfig);
    bool run();

    GstElement* getElementByName(const std::string& name);
    GstPad* getPadFromElement(const std::string& element, const std::string& pad);
    template <typename T>
    void setElementProperty(const std::string& element, const std::string& propertyName, T t);
    void setElementSignalCallBack(const std::string& elementName, const std::string& signalName, GCallback callback, void* data);
    void setBusCallBack(GstBusFunc func, gpointer data);

    GstVideoOverlay* getVideoOverlay(const std::string& elementName, const std::string& overlayName);

private:
    GstElement* m_pipeline { nullptr };
};

#endif // HDDLDEMO_PIPELINEPARSER_H
