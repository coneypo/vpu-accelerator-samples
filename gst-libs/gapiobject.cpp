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
#include "gapiobjectcircle.h"
#include "gapiobjectmosaic.h"

#define g_api_object_parent_class parent_class
G_DEFINE_TYPE(GapiObject, g_api_object, G_TYPE_OBJECT);
static gboolean gapiobject_parse_json(GapiObject *object,
                                      json_object *json_object);
static gboolean gapiobject_parse_gst_structure(GapiObject *object,
        GstStructure *strut);
/* GObject vmethod implementations */

/* initialize the plugin's class */
static void
g_api_object_class_init(GapiObjectClass *klass)
{
    klass->parse_json = gapiobject_parse_json;
    klass->parse_gst_structure = gapiobject_parse_gst_structure;
}

/* initialize the new element
 * initialize instance structure
 */
static void
g_api_object_init(GapiObject *object)
{
    object->meta_id = 0;
    object->meta_type = NULL;
}

static gboolean gapiobject_parse_json(GapiObject *object,
                                      json_object *json_object)
{
    g_assert(object != NULL);
    g_assert(json_object != NULL);
    gapiosd_json_get_uint(json_object, "meta_id", &(object->meta_id));
    object->meta_type = gapiosd_json_get_string(json_object, "meta_type");
    return TRUE;
}
static gboolean gapiobject_parse_gst_structure(GapiObject *object,
        GstStructure *strut)
{
    g_assert(object != NULL);
    g_assert(strut != NULL);
    RETURN_VAL_IF_FAIL(gst_structure_get_uint(strut, "meta_id",
                       &(object->meta_id)), FALSE);
    object->meta_type = gst_structure_get_string(strut, "meta_type");
    return TRUE;
}

gboolean render_sync(GstBuffer *outbuf, GstVideoInfo *sink_info,
                     GstVideoInfo *src_info, gpointer prims_pointer)
{
    g_assert(outbuf != NULL);
    g_assert(sink_info != NULL);
    g_assert(src_info != NULL);
    g_assert(prims_pointer != NULL);

    // judge if have objects need to draw
    if (((cv::gapi::wip::draw::Prims *)prims_pointer)->size() == 0) {
        return TRUE;
    }

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
    },
    {
        "circle",
        g_api_object_circle_get_type(),
        gapiobjectCircle_create
    },
    {
        "mosaic",
        g_api_object_mosaic_get_type(),
        gapiobjectMosaic_create
    }
};

const int gapi_info_map_size = sizeof(gapi_info_map) / sizeof(GAPI_OBJECT_INFO);
