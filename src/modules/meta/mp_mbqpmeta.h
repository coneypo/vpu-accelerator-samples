/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __GST_VIDEO_MBQP_META_H__
#define __GST_VIDEO_MBQP_META_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include "../utils/macros.h"
#include <string.h>
G_BEGIN_DECLS
/**
    GstVideoMBQP:
    @x: x component of upper-left corner
    @y: y component of upper-left corner
    @w: bounding box width
    @h: bounding box height
    @value: QP quality value
*/

typedef struct {
    gint16  x;
    gint16  y;
    guint16 w;
    guint16 h;
    gint8   value;
} GstVideoMBQP;

#ifndef ALIGN_MB            //16*16 / 32*32
#define ALIGN_MB(a,MB_SIZE) ALIGN_POW2(a, MB_SIZE)
#endif

GstVideoMBQP *
gst_video_mbqp_create(gint x, gint y, gint width, gint height, gint value);



GValueArray *
gst_video_mbqp_array_fill(guchar qp_value, int index);


G_END_DECLS
#endif /* __GST_VIDEO_MBQP_META_H__ */
