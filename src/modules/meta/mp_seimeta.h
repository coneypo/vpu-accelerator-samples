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

#ifndef __GST_VIDEO_SEI_META_H__
#define __GST_VIDEO_SEI_META_H__

#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS
/**
    GstSEIMeta:
    @meta: parent #GstMeta
    @sei_type: GQuark describing the semantic of the SEI (f.i. a face, a pedestrian)
    @id: identifier of this particular SEI
    @parent_id: identifier of its parent SEI, used f.i. for SEI hierarchisation.
    @sei_payloads: a array for all the SEI data.

    Extra  buffer metadata describing an image supplemental enhancement information
*/
typedef struct {
    GstMeta    meta;
    GQuark     sei_type;
    gint       id;
    gint       parent_id;
    GPtrArray *sei_payloads;
} GstSEIMeta;

gboolean
gst_buffer_add_sei_meta(GstBuffer *buffer, const guint8 *data,
                        guint32 data_size);

GstSEIMeta *
gst_buffer_get_sei_meta(GstBuffer *buffer);

G_END_DECLS
#endif /* __GST_VIDEO_META_H__ */
