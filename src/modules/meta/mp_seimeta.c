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

#include <string.h>

#include "mp_seimeta.h"

#define VIDEO_SEI_API_TYPE (gst_video_sei_api_get_type())

GType
gst_video_sei_api_get_type(void)
{
    static volatile GType g_type;
    static const gchar *tags[] = {
        GST_META_TAG_VIDEO_STR,
        GST_META_TAG_VIDEO_ORIENTATION_STR,
        GST_META_TAG_VIDEO_SIZE_STR, NULL
    };

    if (g_once_init_enter(&g_type)) {
        GType type =
            gst_meta_api_type_register("GstVideoSEIMetaAPI", tags);
        GST_INFO("Registering Video SEI Meta API");
        g_once_init_leave(&g_type, type);
    }

    return g_type;
}

static gboolean
gst_video_sei_meta_init(GstMeta *meta, gpointer params, GstBuffer *buffer)
{
    GstSEIMeta *sei_meta = (GstSEIMeta *) meta;
    sei_meta->id = 0;
    sei_meta->parent_id = 0;
    sei_meta->sei_payloads = g_ptr_array_new_with_free_func((
                                 GDestroyNotify)g_array_unref);
    sei_meta->sei_type = g_quark_from_string("sei");
    return TRUE;
}

static void
gst_video_sei_meta_free(GstMeta *meta, GstBuffer *buffer)
{
    GstSEIMeta *sei_meta = (GstSEIMeta *) meta;
    g_ptr_array_unref(sei_meta->sei_payloads);
}

const GstMetaInfo *
gst_video_sei_meta_get_info(void)
{
    static const GstMetaInfo *meta_info = NULL;

    if (g_once_init_enter(&meta_info)) {
        const GstMetaInfo *mi = gst_meta_register(
                                    VIDEO_SEI_API_TYPE, "GstSEIMeta", sizeof(GstSEIMeta),
                                    gst_video_sei_meta_init, gst_video_sei_meta_free, NULL);
        g_once_init_leave(&meta_info, mi);
    }

    return meta_info;
}

#define VIDEO_SEI_META_INFO (gst_video_sei_meta_get_info())

#define NEW_SEI_META(buffer) \
    ((GstSEIMeta*)gst_buffer_add_meta (buffer, VIDEO_SEI_META_INFO, NULL))

gboolean
gst_buffer_add_sei_meta(GstBuffer *buffer, const guint8 *data,
                        guint32 data_size)
{
    if (!buffer || !GST_IS_BUFFER(buffer) || !data || !data_size) {
        return FALSE;
    }

    GstSEIMeta *meta = gst_buffer_get_sei_meta(buffer);

    if (!meta) {
        meta = NEW_SEI_META(buffer);
    }

    GArray *array = g_array_new(FALSE, FALSE, sizeof(guint8));
    g_array_append_vals(array, data, data_size);
    g_ptr_array_add(meta->sei_payloads, array);
    return TRUE;
}

GstSEIMeta *
gst_buffer_get_sei_meta(GstBuffer *buffer)
{
    return ((GstSEIMeta *)gst_buffer_get_meta((buffer), VIDEO_SEI_API_TYPE));
}
