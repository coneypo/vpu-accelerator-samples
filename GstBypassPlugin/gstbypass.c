/*
 * Copyright (c) 2019 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//!
//! \file    gstbypass.c
//! \brief   GStreamer plugins for HDDL Bypass Mode
//! \details Gstreamer plugins for initiate WorkloadContext
//!

#include "gstbypass.h"
#include <gst/gststructure.h>
#include <gst/gstquery.h>

#include <unistd.h>
#include <stdio.h>
#include <sys/syscall.h>

/* FIXME* update PACKAGE_ORIGIN after opened source */
#define PACKAGE_ORIGIN "http://www.intel.com"
#define PACKAGE_VERSION "0.0.1.0"
#define PLUGIN_LICENSE "MIT/X11"

#define PACKAGE "gstbypass"
#define GST_PLUGIN_NAME "bypass"
#define GST_PLUGIN_DESC "Bypass helper for setting up workload"

GST_DEBUG_CATEGORY_STATIC (gst_bypass_debug);
#define GST_CAT_DEFAULT gst_bypass_debug

#define gst_bypass_parent_class parent_class
G_DEFINE_TYPE (GstBypass, gst_bypass, GST_TYPE_ELEMENT);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY"));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY"));

static gboolean gst_bypass_sink_query (GstPad    *pad,
                                         GstObject *parent,
                                         GstQuery  *query){
    GST_DEBUG("Query received by bypass\n");
    gboolean ret = FALSE;
    GstBypass *obj = GST_BYPASS (parent);

    switch(GST_QUERY_TYPE (query)){
        case GST_QUERY_CUSTOM:
            GST_DEBUG("Custom query received by bypass\n");
            GstStructure *s = gst_query_writable_structure (query);

            const gchar *q_type = gst_structure_get_string (s, "BypassQueryType");
            if (q_type != NULL && g_str_equal (q_type, "WorkloadContextQuery")){
                WorkloadID wID = obj->wID;
                if(wID == NULL){
                    GST_ERROR("Fail to get workload id in query\n");
                }
                gst_structure_set (s, "WorkloadContextId", G_TYPE_UINT64, (guint64)wID, NULL);
                GST_DEBUG("WorkloadCtx set to query by bypass\n");
                ret = TRUE;
            }
            break;
        default:
            ret = gst_pad_query_default (pad, parent, query);
            break;
    }
    obj->isWIDExported = TRUE;
    return ret;
}

static GstFlowReturn gst_bypass_chain (GstPad *pad, GstObject *parent, GstBuffer *buf)
{
    GstBypass *bypass = GST_BYPASS (parent);

    gst_pad_push (bypass->srcpad, buf);
}

static void gst_bypass_finalize (GObject *object)
{
    GST_DEBUG ("pid %u tid %lu\n", getpid(), syscall (SYS_gettid));
    WorkloadID workloadId;
    HddlStatusCode hddlStatus = HDDL_OK;

    hddlStatus = getWorkloadContextId (getpid (), syscall (SYS_gettid), &workloadId);

    if (hddlStatus != HDDL_OK)
    {
        GST_ERROR ("Failed to obtain workload %ld\n", workloadId);
    }

    if(!GST_BYPASS(object)->isWIDExported && workloadId != 0){
        destroyWorkloadContext(workloadId);

        GST_DEBUG ("Destroyed workload %lu with pid %u tid %lu\n", workloadId, getpid(),
        syscall (SYS_gettid));
    }

    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void gst_bypass_dispose (GObject *object)
{
    GST_DEBUG ("pid %u tid %lu\n", getpid(), syscall (SYS_gettid));

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

static WorkloadID gst_bypass_workload_init ()
{
    WorkloadID workloadId;
    ContextHint contextHint;
    HddlStatusCode hddlStatus = HDDL_OK;

    // FIXME: Hardcoded context hint. Fix it later.
    contextHint.mediaDecodeType = MEDIA_DECODE_TYPE_H264;
    contextHint.mediaBitrate = 2.1;
    contextHint.ResolutionWidth = 640;
    contextHint.ResolutionHeight = 480;
    contextHint.mediaFps = 29.7;
    contextHint.internalWorkload = 0;

    hddlStatus = getWorkloadContextId (getpid (), syscall (SYS_gettid), &workloadId);

    if (hddlStatus != HDDL_OK)
    {
        GST_DEBUG ("Fail to get WID from pid+tid. New WID to be created\n");

        hddlStatus = createWorkloadContext (&workloadId, &contextHint);

        if (hddlStatus != HDDL_OK)
        {
	    GST_ERROR ("Failed to create workload context\n");
        return NULL;
        }

        GST_DEBUG ("Created workload %lu with pid %u tid %lu\n", workloadId, getpid(),
	    syscall (SYS_gettid));
    }

    return workloadId;
}

static void gst_bypass_class_init (GstBypassClass *klass)
{
    GST_DEBUG ("pid %u tid %lu\n", getpid(), syscall (SYS_gettid));
    // klass->wID = gst_bypass_workload_init ();
    // if(klass->wID == NULL){
    //     GST_ERROR ("Failed to get workload context in class init\n");
    // }

    GObjectClass *const gobjectClass = G_OBJECT_CLASS (klass);
    GstElementClass *const elementClass = GST_ELEMENT_CLASS (klass);

    gobjectClass->finalize = gst_bypass_finalize;
    gobjectClass->dispose = gst_bypass_dispose;

    gst_element_class_set_static_metadata (elementClass, "Bypass", "Generic",
        GST_PLUGIN_DESC, "Soon, Thean Siew <thean.siew.soon@intel.com>");

    gst_element_class_add_pad_template (elementClass,
        gst_static_pad_template_get (&sink_factory));
    gst_element_class_add_pad_template (elementClass,
        gst_static_pad_template_get (&src_factory));

}

static void gst_bypass_init (GstBypass *bypass)
{
    GST_DEBUG ("pid %u tid %lu\n", getpid(), syscall (SYS_gettid));
    bypass->wID = gst_bypass_workload_init ();
    if(bypass->wID == NULL){
        GST_ERROR ("Failed to get workload context in class init\n");
    }
    bypass->isWIDExported = FALSE;

    bypass->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
    gst_pad_set_chain_function (bypass->sinkpad, gst_bypass_chain);
    GST_PAD_SET_PROXY_CAPS (bypass->sinkpad);
    gst_element_add_pad (GST_ELEMENT (bypass), bypass->sinkpad);

    gst_pad_set_query_function (bypass->sinkpad, gst_bypass_sink_query);

    bypass->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
    GST_PAD_SET_PROXY_CAPS (bypass->srcpad);
    gst_element_add_pad (GST_ELEMENT (bypass), bypass->srcpad);

    gst_pad_set_query_function (bypass->srcpad, gst_bypass_sink_query);
}


static gboolean bypass_plugin_init (GstPlugin *plugin)
{

    gboolean ret = FALSE;
    GST_DEBUG_CATEGORY_INIT (gst_bypass_debug, GST_PLUGIN_NAME, 0, GST_PLUGIN_DESC);

    ret = gst_element_register (plugin, "bypass", GST_RANK_NONE, GST_TYPE_BYPASS);

    GST_DEBUG ("pid %u tid %lu\n", getpid(), syscall (SYS_gettid));
    // gst_bypass_workload_init ();

    return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, bypass, GST_PLUGIN_DESC,
    bypass_plugin_init, PACKAGE_VERSION, PLUGIN_LICENSE, PACKAGE, PACKAGE_ORIGIN);
