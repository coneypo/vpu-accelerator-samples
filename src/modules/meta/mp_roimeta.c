/* * MIT License
 *
 * Copyright (c) 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "mp_roimeta.h"
#include <string.h>

GstVideoROI *
gst_video_roi_create(gint x, gint y, gint width, gint height, gint value)
{
    GstVideoROI *roi = g_new0(GstVideoROI, 1);
    roi->x = x;
    roi->y = y;
    roi->w = width;
    roi->h = height;
    roi->value = value;
    return roi;
}

#define VIDEO_ROI_API_TYPE (gst_video_roi_meta_api_get_type())

GType
gst_video_roi_meta_api_get_type(void)
{
    static volatile GType g_type;
    static const gchar *tags[] = {
        GST_META_TAG_VIDEO_STR,
        GST_META_TAG_VIDEO_ORIENTATION_STR,
        GST_META_TAG_VIDEO_SIZE_STR, NULL
    };

    if (g_once_init_enter(&g_type)) {
        GType type = gst_meta_api_type_register("GstVideoROIMetaAPI", tags);
        GST_INFO("Registering Video ROI Meta API");
        g_once_init_leave(&g_type, type);
    }

    return g_type;
}

static gboolean
gst_video_roi_meta_init(GstMeta *meta, gpointer params, GstBuffer *buffer)
{
    GstROIMeta *roi_meta = (GstROIMeta *) meta;
    roi_meta->id = 0;
    roi_meta->parent_id = 0;
    roi_meta->roi_list = NULL;
    return TRUE;
}

static void
gst_video_roi_meta_free(GstMeta *meta, GstBuffer *buffer)
{
    // we don't need to do anything here
    // because mediapipe will destroy the roi_list when destroy_context()
}

#define VIDEO_ROI_META_INFO (gst_video_roi_meta_get_info())

static const GstMetaInfo *
gst_video_roi_meta_get_info(void)
{
    static const GstMetaInfo *meta_info = NULL;

    if (g_once_init_enter(&meta_info)) {
        const GstMetaInfo *mi = gst_meta_register(
                                    VIDEO_ROI_API_TYPE, "GstVideoROIMeta", sizeof(GstROIMeta),
                                    gst_video_roi_meta_init, gst_video_roi_meta_free, NULL);
        g_once_init_leave(&meta_info, mi);
    }

    return meta_info;
}

gboolean
gst_buffer_add_video_roi_meta(GstBuffer *buffer, GList *roi_list,
                              const gchar *roi_type)
{
    return gst_buffer_add_video_roi_meta_id(buffer, roi_list,
                                            g_quark_from_string(roi_type));
}

#define GST_BUFFER_NEW_ROI_META(buf) \
    ((GstROIMeta*)gst_buffer_add_meta (buffer, VIDEO_ROI_META_INFO, NULL))

gboolean
gst_buffer_add_video_roi_meta_id(GstBuffer *buffer, GList *roi_list,
                                 GQuark roi_type)
{
    g_return_val_if_fail(GST_IS_BUFFER(buffer), FALSE);
    GstROIMeta *meta = gst_buffer_get_video_roi_meta(buffer);

    if (!meta) {
        meta = GST_BUFFER_NEW_ROI_META(buffer);
        meta->roi_type = roi_type;
    }

    if (meta->roi_list) {
        meta->roi_list = g_list_concat(meta->roi_list, roi_list);
    } else {
        meta->roi_list = roi_list;
    }

    return TRUE;
}

GstROIMeta *
gst_buffer_get_video_roi_meta(GstBuffer *buffer)
{
    return ((GstROIMeta *)gst_buffer_get_meta((buffer), VIDEO_ROI_API_TYPE));
}
