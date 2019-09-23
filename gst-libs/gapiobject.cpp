/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

/**
 * SECTION:element-plugin
 *
 * FIXME:Describe plugin here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! plugin ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "common.h"
#include <new>
#include "gapiobject.h"
#include "gapiobjecttext.h"
#include "gapiobjectrectangle.h"
#include "gapiobjectline.h"

#define g_api_object_parent_class parent_class
G_DEFINE_TYPE(GapiObject, g_api_object, G_TYPE_OBJECT);

/* GObject vmethod implementations */

/* initialize the plugin's class */
static void
g_api_object_class_init(GapiObjectClass *klass)
{
    /* GObjectClass* gobject_class; */
    /* gobject_class = (GObjectClass*) klass; */
}

/* initialize the new element
 * initialize instance structure
 */
static void
g_api_object_init(GapiObject *object)
{
}


gboolean render_sync(GstBuffer *outbuf, GstVideoInfo *sink_info,
                     GstVideoInfo *src_info, gpointer prims_pointer)
{
    g_assert(outbuf != NULL);
    g_assert(sink_info != NULL);
    g_assert(src_info != NULL);
    g_assert(prims_pointer != NULL);
    GstMapInfo info;
    GstMapFlags mapFlag = GstMapFlags(GST_MAP_READ | GST_MAP_WRITE);
    //map buf to info
    if (! gst_buffer_map(outbuf, &info, mapFlag)) {
        GST_ERROR("render_sync error! buffer map error!\n");
        return FALSE;
    }
    guint8 *pic = info.data;
    int video_width = GST_VIDEO_INFO_WIDTH(src_info);
    int video_height = GST_VIDEO_INFO_HEIGHT(src_info);
    //bgr format
    if (GST_VIDEO_INFO_FORMAT(src_info) == GST_VIDEO_FORMAT_BGR) {
        cv::Mat bgr(video_height, video_width, CV_8UC3, pic);
        cv::gapi::wip::draw::render(bgr,
                *((cv::gapi::wip::draw::Prims *)prims_pointer));
    }//nv12 format
    else if (GST_VIDEO_INFO_FORMAT(src_info) == GST_VIDEO_FORMAT_NV12) {
        cv::Mat y_plane(video_height, video_width, CV_8UC1, pic);
        cv::Mat uv_plane(video_height/2, video_width/2, CV_8UC2,(pic + (video_width * video_height)));
        cv::gapi::wip::draw::render(y_plane, uv_plane, *((cv::gapi::wip::draw::Prims *)prims_pointer));
    }//other format
    else {
        GST_ERROR("render_sync error! format not support!\n");
    }
    gst_buffer_unmap(outbuf, &info);
    ((cv::gapi::wip::draw::Prims *)prims_pointer)->clear();
    return TRUE;
}
gpointer init_array()
{
    gpointer prims_pointer = new cv::gapi::wip::draw::Prims;
    return prims_pointer;
}
void destory_array(gpointer prims_pointer)
{
    if(NULL != prims_pointer) {
        delete (cv::gapi::wip::draw::Prims *)prims_pointer;
    }
    return;
}

GAPI_OBJECT_INFO gapi_info_map[] = {
    {
        "text",
        g_api_object_text_get_type(),
        gapiobjectText_create
    },
    {
        "rect",
        g_api_object_rect_get_type(),
        gapiobjectRect_create
    },
    {
        "line",
        g_api_object_line_get_type(),
        gapiobjectLine_create
    }
};

const int gapi_info_map_size = sizeof(gapi_info_map) / sizeof(GAPI_OBJECT_INFO);
