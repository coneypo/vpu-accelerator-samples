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
