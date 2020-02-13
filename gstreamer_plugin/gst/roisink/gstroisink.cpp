/* GStreamer
 * Copyright (C) 2020 FIXME <fixme@example.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstroisink
 *
 * The roisink element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v fakesrc ! roisink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */
#include "gstroisink.h"
#include "infermetasender.h"

#include <gst/app/gstappsink.h>
#include <gst/base/gstbasesink.h>
#include <gst/gst.h>
#include <gst/video/gstvideometa.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

GST_DEBUG_CATEGORY_STATIC(gst_roisink_debug_category);
#define GST_CAT_DEFAULT gst_roisink_debug_category

/* prototypes */

static void gst_roisink_set_property(GObject* object,
    guint property_id, const GValue* value, GParamSpec* pspec);
static void gst_roisink_get_property(GObject* object,
    guint property_id, GValue* value, GParamSpec* pspec);
static gboolean gst_roisink_query(GstBaseSink* sink, GstQuery* query);

static gboolean gst_roisink_event(GstPad* pad, GstObject* parent, GstEvent* event);

static GstFlowReturn new_sample(GstElement* sink, gpointer data);

enum {
    PROP_0,
    PROP_SOCKET_NAME
};

/* pad templates */

static GstStaticPadTemplate gst_roisink_sink_template = GST_STATIC_PAD_TEMPLATE("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("ANY"));

/* class initialization */
#define gst_roisink_parent_class parent_class

G_DEFINE_TYPE_WITH_CODE(GstRoiSink, gst_roisink, GST_TYPE_APP_SINK,
    GST_DEBUG_CATEGORY_INIT(gst_roisink_debug_category, "roisink", 0,
        "debug category for roisink element"));

static void
gst_roisink_class_init(GstRoiSinkClass* klass)
{
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    GstBaseSinkClass* base_sink_class = GST_BASE_SINK_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
    gst_element_class_add_static_pad_template(GST_ELEMENT_CLASS(klass),
        &gst_roisink_sink_template);

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
        "FIXME Long name", "Generic", "FIXME Description",
        "FIXME <fixme@example.com>");

    gobject_class->set_property = gst_roisink_set_property;
    gobject_class->get_property = gst_roisink_get_property;
    base_sink_class->query = GST_DEBUG_FUNCPTR(gst_roisink_query);

    g_object_class_install_property(gobject_class, PROP_SOCKET_NAME,
        g_param_spec_string("socketname", "SocketName", "unix socket file name",
            NULL, G_PARAM_READWRITE));
}

static void
gst_roisink_init(GstRoiSink* roisink)
{
    roisink->sender = new InferMetaSender();
    roisink->isConnected = FALSE;
    GstBaseSink* baseSink = GST_BASE_SINK(roisink);

    gst_pad_set_event_function(baseSink->sinkpad,
        GST_DEBUG_FUNCPTR(gst_roisink_event));

    gst_app_sink_set_emit_signals(GST_APP_SINK(roisink), TRUE);
    g_signal_connect(GST_APP_SINK(roisink), "new-sample", G_CALLBACK(new_sample), NULL);
}

static void gst_roisink_finalize(GstRoiSink* roisink)
{
    if (roisink->sender) {
        delete roisink->sender;
    }
}

void gst_roisink_set_property(GObject* object, guint property_id,
    const GValue* value, GParamSpec* pspec)
{
    GstRoiSink* roisink = GST_ROISINK(object);

    GST_DEBUG_OBJECT(roisink, "set_property");

    switch (property_id) {
    case PROP_SOCKET_NAME:
        roisink->socketName = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_roisink_get_property(GObject* object, guint property_id,
    GValue* value, GParamSpec* pspec)
{
    GstRoiSink* roisink = GST_ROISINK(object);

    GST_DEBUG_OBJECT(roisink, "get_property");
    switch (property_id) {
    case PROP_SOCKET_NAME:
        g_value_set_string(value, roisink->socketName);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

/* notify subclass of query */
static gboolean
gst_roisink_query(GstBaseSink* sink, GstQuery* query)
{
    GstRoiSink* roisink = GST_ROISINK(sink);

    GST_DEBUG_OBJECT(roisink, "query");
    switch (GST_QUERY_TYPE(query)) {
    default: {
        GstElement* parent = GST_ELEMENT(&roisink->parent);
        return GST_ELEMENT_CLASS(parent_class)->query(parent, query);
    }
    }
}

static gboolean gst_roisink_event(GstPad* pad, GstObject* parent, GstEvent* event)
{
    gboolean ret = true;
    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_STREAM_START: {
        GstRoiSink* roiSink = GST_ROISINK(parent);
        if (roiSink->socketName) {
            roiSink->isConnected = roiSink->sender->connectServer(roiSink->socketName);
            if (!roiSink->isConnected) {
                GST_WARNING("falied to connect server: %s \n", roiSink->socketName);
            }
        }
    }
    default:
        ret = gst_pad_event_default(pad, parent, event);
        break;
    }
    return ret;
}

GstFlowReturn new_sample(GstElement* sink, gpointer data)
{
    GstSample* sample;
    GstRoiSink* roiSink = GST_ROISINK(sink);

    /* Retrieve the buffer */
    g_signal_emit_by_name(sink, "pull-sample", &sample);

    if (sample) {
        GstBuffer* metaBuffer = gst_sample_get_buffer(sample);
        if (metaBuffer) {
            GstVideoRegionOfInterestMeta* meta_orig = NULL;
            gpointer state = NULL;
            gboolean needSend = FALSE;

            while ((meta_orig = (GstVideoRegionOfInterestMeta*)gst_buffer_iterate_meta_filtered(metaBuffer, &state, gst_video_region_of_interest_meta_api_get_type()))) {
                std::string label = "null";
                if (g_quark_to_string(meta_orig->roi_type)) {
                    label = g_quark_to_string(meta_orig->roi_type);
                }
                roiSink->sender->serializeSave(meta_orig->x, meta_orig->y, meta_orig->w, meta_orig->h, label, GST_BUFFER_PTS(metaBuffer), 1.0);
                needSend = TRUE;
            }
            if (needSend && roiSink->isConnected) {
                roiSink->sender->send();
            }
        }
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }
    return GST_FLOW_ERROR;
}

static gboolean
plugin_init(GstPlugin* plugin)
{

    /* FIXME Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
    return gst_element_register(plugin, "roisink", GST_RANK_NONE,
        GST_TYPE_ROISINK);
}

/* FIXME: these are normally defined by the GStreamer build system.
   If you are creating an element to be included in gst-plugins-*,
   remove these, as they're always defined.  Otherwise, edit as
   appropriate for your external plugin package. */
#ifndef VERSION
#define VERSION "0.0.FIXME"
#endif
#ifndef PACKAGE
#define PACKAGE "FIXME_package"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "FIXME_package_name"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://FIXME.org/"
#endif

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    roisink,
    "FIXME plugin description",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

#ifdef __cplusplus
}
#endif
