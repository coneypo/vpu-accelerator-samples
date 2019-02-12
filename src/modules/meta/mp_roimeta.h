/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __GST_VIDEO_ROI_META_H__
#define __GST_VIDEO_ROI_META_H__

#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS
/**
    GstVideoROI:
    @x: x component of upper-left corner
    @y: y component of upper-left corner
    @w: bounding box width
    @h: bounding box height
    @value: ROI quality value
*/
typedef struct {
    gint16  x;
    gint16  y;
    guint16 w;
    guint16 h;
    gint8   value;
} GstVideoROI;

/**
    GstROIMeta:
    @meta: parent #GstMeta
    @roi_type: GQuark describing the semantic of the Roi (f.i. a face, a pedestrian)
    @id: identifier of this particular ROI
    @parent_id: identifier of its parent ROI, used f.i. for ROI hierarchisation.
    @roi_list: list of GstVideoROI data
*/
typedef struct {
    GstMeta meta;
    GQuark  roi_type;
    gint    id;
    gint    parent_id;
    GList  *roi_list;
} GstROIMeta;

#define gst_video_roi_destroy g_free

GstVideoROI *
gst_video_roi_create(gint x, gint y, gint width, gint height, gint value);

gboolean
gst_buffer_add_video_roi_meta(GstBuffer *buffer, GList *roi_list,
                              const gchar *roi_type);
//gst_buffer_add_video_roi_meta (GstBuffer* buffer, GList* roi_list, const gchar* roi_type = "oclroimetainfo");

gboolean
gst_buffer_add_video_roi_meta_id(GstBuffer *buffer, GList *roi_list,
                                 GQuark roi_type);

GstROIMeta *
gst_buffer_get_video_roi_meta(GstBuffer *buffer);

G_END_DECLS
#endif /* __GST_VIDEO_ROI_META_H__ */
