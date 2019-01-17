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

#ifndef __MEDIAPIPE_BRANCH_H__
#define __MEDIAPIPE_BRANCH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "mediapipe_com.h"

typedef struct {
    GstPad *probe_pad;
    gulong  probe_id;
} prob_item_t;

typedef struct mediapipe_branch_s  {
    guint input_width;
    guint input_height;
    GstElement *pipeline;
    GstElement *source;
    GList *probe_items;
    guint bus_watch_id;
    gboolean(*branch_init)(struct mediapipe_branch_s  *);
} mediapipe_branch_t;

void
gst_probe_item_destroy(gpointer data);

gboolean
gst_probe_list_append_new_item(GList *list, GstPad *pad, gulong id);

void
gst_probe_list_destroy(GList *list);

gboolean mediapipe_branch_push_buffer(mediapipe_branch_t  *branch,
                                      GstBuffer *buffer);

gboolean
mediapipe_setup_new_branch(mediapipe_t *mp, const gchar *element_name,
                           const gchar *pad_name, mediapipe_branch_t  *branch);

GstElement *
mediapipe_branch_create_pipeline(const gchar *pipeline_description);

gboolean
mediapipe_branch_start(mediapipe_branch_t  *branch);

void
mediapipe_branch_destroy_internal(mediapipe_branch_t  *branch);

gboolean
mediapipe_branch_reconfig(mediapipe_branch_t  *branch, guint width, guint height);

#ifdef __cplusplus
}
#endif

#endif
