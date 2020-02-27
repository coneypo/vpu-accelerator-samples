//
// Created by xiao on 2020/2/19.
//

#include "pipeline.h"
#include "fpsstat.h"

#include <gio/gio.h>
#include <gst/gstclock.h>

#define OVERLAY_NAME "mfxsink0"

static void timerAsync(gint32 timeout, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData);
static void timerThread(GTask* task, gpointer source, gpointer userData, GCancellable* cancellable);
static void timerCallBack(GObject* source, GAsyncResult* res, gpointer userData);

Pipeline::~Pipeline()
{
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
    }
}

bool Pipeline::parse(const char* pipeline, const char* displaySink)
{
    //parse gstreamer pipeline
    gst_init(NULL, NULL);
    m_pipeline = gst_parse_launch_full(pipeline, NULL, GST_PARSE_FLAG_FATAL_ERRORS, NULL);
    m_displaySink = gst_bin_get_by_name(GST_BIN(m_pipeline), displaySink);
    m_overlay = GST_VIDEO_OVERLAY(gst_bin_get_by_name(GST_BIN(m_displaySink), OVERLAY_NAME));

    //set bus callback
    GstBus* bus = gst_element_get_bus(m_pipeline);
    gst_bus_add_watch(bus, Pipeline::busCallBack, this);
    gst_object_unref(bus);

    //probe sinkpad for calculate fps
    GstPad* sinkPad = gst_element_get_static_pad(m_displaySink, "sink");
    if (!sinkPad) {
        return false;
    }
    m_fpsProb = std::make_shared<FpsStat>(sinkPad);
    return true;
}

bool Pipeline::run(guintptr winhandler, int timeout)
{
    if (m_overlay) {
        gst_video_overlay_set_window_handle(m_overlay, winhandler);
    }
    auto ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);

    if (timeout > 0) {
        GCancellable* cancellable = g_cancellable_new();
        timerAsync(timeout, cancellable, timerCallBack, m_pipeline);
        g_object_unref(cancellable);
    }

    return !(ret == GST_STATE_CHANGE_FAILURE);
}

void Pipeline::process(PipelineAction action)
{
    switch (action) {
    case MESSAGE_START:
        gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        break;
    case MESSAGE_STOP:
        gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
        break;
    case MESSAGE_REPLAY: {
        gst_element_seek_simple(m_pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_KEY_UNIT, 0);
        gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        break;
    }
    default:
        GST_ERROR("Unrecognized action\n");
    }
}

void Pipeline::exposeVideoOverlay()
{
    if (m_overlay) {
        gst_video_overlay_expose(m_overlay);
    }
}

double Pipeline::getFps()
{
    double fps = 0.0;
    if (m_fpsProb) {
        fps = m_fpsProb->getRenderFps();
    }
    return fps;
}
gboolean Pipeline::busCallBack(GstBus* bus, GstMessage* msg, gpointer data)
{
    Pipeline* obj = (Pipeline*)data;
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError* err = NULL;
        gchar* dbg = NULL;
        gst_message_parse_error(msg, &err, &dbg);
        if (err) {
            GST_ERROR("ERROR: %s", err->message);
            g_error_free(err);
        }
        if (dbg) {
            GST_DEBUG("Debug details: %s ", dbg);
            g_free(dbg);
        }
        break;
    }

    case GST_MESSAGE_EOS:
        GST_DEBUG("EOS");
        g_print("eos\n");
        obj->m_fpsProb->reset();
        gst_element_seek_simple(obj->m_pipeline, GST_FORMAT_TIME, (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT ) , 0*GST_SECOND);
        gst_element_set_state(obj->m_pipeline,GST_STATE_PLAYING);
        break;
    default:
        break;
    }
    return true;
}

void timerAsync(gint32 timeout, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    GTask* task = g_task_new(NULL, cancellable, callback, userData);
    g_task_set_task_data(task, GINT_TO_POINTER(timeout), NULL);
    g_task_run_in_thread(task, timerThread);
    g_object_unref(task);
}

void timerThread(GTask* task, gpointer source, gpointer userData, GCancellable* cancellable)
{
    GstClock* clock;
    GstClockID id;
    GstClockTime base;
    gint32 time = GPOINTER_TO_INT(userData);

    clock = gst_system_clock_obtain();
    base = gst_clock_get_time(clock);
    id = gst_clock_new_single_shot_id(clock, base + time * GST_SECOND);
    gst_clock_id_wait(id, NULL);
    gst_clock_id_unschedule(id);
    gst_clock_id_unref(id);
    gst_object_unref(clock);
}

void timerCallBack(GObject* source, GAsyncResult* res, gpointer userData)
{
    GstElement* pipeline = GST_ELEMENT(userData);
    gst_element_set_state(pipeline, GST_STATE_PAUSED);
}
