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


#include "mp_branch.h"

#define MAX_BUF_SIZE 512
void
gst_probe_item_destroy(gpointer data)
{
    prob_item_t *item = (prob_item_t *) data;

    if (item == NULL) {
        return;
    }

    gst_pad_remove_probe(item->probe_pad, item->probe_id);
    gst_object_unref(item->probe_pad);
    g_free(item);
}

gboolean
gst_probe_list_append_new_item(GList *list, GstPad *pad, gulong id)
{
    if (pad == NULL || id == 0) {
        return FALSE;
    }

    prob_item_t *item = g_new0(prob_item_t, 1);
    item->probe_pad = pad;
    item->probe_id = id;
    list = g_list_append(list, item);
    return TRUE;
}


void
gst_probe_list_destroy(GList *list)
{
    if (list == NULL) {
        return;
    }

    g_list_free_full(list, gst_probe_item_destroy);
    list = NULL;
}

static gboolean
pipeline_is_usable(GstElement *pipeline)
{
    GstStateChangeReturn ret;

    if (pipeline == NULL) {
        return FALSE;
    }

    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        return FALSE;
    }

    ret = gst_element_set_state(pipeline, GST_STATE_NULL);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis create pipeline from string
 *
 * @Param pipeline_description
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
GstElement *
mediapipe_branch_create_pipeline(const gchar *pipeline_description)
{
    GError *error = NULL;
    GstElement *pipeline = gst_parse_launch(pipeline_description, &error);

    if (error) {
        g_clear_error(&error);
        return NULL;
    }

    if (pipeline_is_usable(pipeline) == FALSE) {
        g_object_unref(pipeline);
        return NULL;
    }

    return pipeline;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis let branch pipeline to run
 *
 * @Param branch
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
gboolean
mediapipe_branch_start(mediapipe_branch_t *branch)
{
    if (branch == NULL || branch->pipeline == NULL) {
        return FALSE;
    }

    GstStateChangeReturn ret =
        gst_element_set_state(branch->pipeline, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        return FALSE;
    }

    return TRUE;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis Destroy branch
 *
 * @Param branch
 */
/* ----------------------------------------------------------------------------*/
void
mediapipe_branch_destroy_internal(mediapipe_branch_t *branch)
{
    if (branch == NULL) {
        return;
    }

    gst_probe_list_destroy(branch->probe_items);

    if (branch->pipeline) {
        gst_element_set_state(branch->pipeline, GST_STATE_NULL);
        gst_object_unref(branch->pipeline);
    }

    if (branch->source) {
        gst_object_unref(branch->source);
    }

    if (branch->bus_watch_id) {
        g_source_remove(branch->bus_watch_id);
    }

    branch = NULL;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis reconfig branch( haven't use )
 *
 * @Param branch
 * @Param width
 * @Param height
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
gboolean
mediapipe_branch_reconfig(mediapipe_branch_t *branch, guint width, guint height)
{
    gchar desc[MAX_BUF_SIZE];
    GstCaps *caps;

    if (branch->input_width == width && branch->input_height == height) {
        return TRUE;
    }

    snprintf(desc, MAX_BUF_SIZE,
             "video/x-raw,format=NV12,width=%u,height=%u,framerate=30/1",
             width, height);
    caps = gst_caps_from_string(desc);
    gst_element_set_state(branch->source, GST_STATE_NULL);
    g_object_set(branch->source, "caps", caps, NULL);
    gst_element_set_state(branch->source, GST_STATE_PLAYING);
    gst_caps_unref(caps);
    return TRUE;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis push buffer to this branch
 *
 * @Param branch
 * @Param buffer
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
gboolean
mediapipe_branch_push_buffer(mediapipe_branch_t *branch, GstBuffer *buffer)
{
    if (!branch && !buffer) {
        g_print("branch or buffer is null when push buffer to branch\n");
        return FALSE;
    }

    if (branch && branch->source) {
        gst_app_src_push_buffer(GST_APP_SRC(branch->source),
                                gst_buffer_copy(buffer));
    }

    return TRUE;
}



