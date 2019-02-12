/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mediapipe_com.h"
typedef struct {
    GstElement *tee;
    GstElement *queue;
    GstPad *srcpad;
    GstPad *sinkpad;
} custom_link_data_t;

static void
custom_link_data_destroy(custom_link_data_t *link_data);

static GstPadProbeReturn
attach_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);

static GstPadProbeReturn
detach_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);

static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata);

static int mediapipe_detach_channel(mediapipe_t *mp, const gchar *tee_name,
                                    guint8 branch_id, const char *queue_name);
static int mediapipe_attach_channel(mediapipe_t *mp, const gchar *tee_name,
                                    guint8 branch_id, const char *queue_name);

static mp_command_t  mp_channel_commands[] = {
    {
        mp_string("channel"),
        MP_MAIN_CONF,
        NULL,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_core_module_t  mp_channel_module_ctx = {
    mp_string("channel"),
    NULL,
    NULL
};


mp_module_t  mp_channel_module = {
    MP_MODULE_V1,
    &mp_channel_module_ctx,                /* module context */
    mp_channel_commands,                   /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    keyshot_process,                    /* keyshot_process*/
    NULL,                               /* message_process */
    NULL,                               /* init_callback */
    NULL,                               /* netcommand_process */
    NULL,                        /* exit master */
    MP_MODULE_V1_PADDING
};



static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata)
{
    mp_int_t ret = MP_OK;

    if (userdata == NULL) {
        return MP_ERROR;
    }

    char *key = (char *) userdata;

    if (key[0]=='?') {
        printf(" ===== 'd' : detach channel0                             =====\n");
        printf(" ===== 'g' : attach channel0                             =====\n");
        return MP_IGNORE;
    }

    if (key[0] != 'd' && key[0] != 'g') {
        return MP_IGNORE;
    }

    if (key[0] == 'd') {
        mediapipe_detach_channel(mp, "tt", 0, "q0");
    } else if (key[0] == 'g') {
        mediapipe_attach_channel(mp, "tt", 0, "q0");
    }

    return ret;
}

static GstPadProbeReturn
attach_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    custom_link_data_t *link_data = (custom_link_data_t *)user_data;
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    gst_pad_link(link_data->srcpad, link_data->sinkpad);
    return GST_PAD_PROBE_REMOVE;
}

int mediapipe_attach_channel(mediapipe_t *mp, const gchar *tee_name,
                             guint8 branch_id, const char *queue_name)
{
    custom_link_data_t *link_data = g_new0(custom_link_data_t, 1);
    link_data->tee = gst_bin_get_by_name(GST_BIN(mp->pipeline), tee_name);

    if (link_data->tee == NULL) {
        LOG_ERROR("Failed to get tee element named \'%s\'", tee_name);
        g_free(link_data);
        return -1;
    }

    gchar pad_name[16];
    sprintf(pad_name, "src_%u", branch_id);
    link_data->srcpad = gst_element_get_static_pad(link_data->tee, pad_name);

    if (link_data->srcpad != NULL) {
        LOG_ERROR("There is already a static srcpad named \'%s\' on element \'%s\'",
                  pad_name, tee_name);
        custom_link_data_destroy(link_data);
        return -1;
    }

    GstPadTemplate *templ = gst_element_class_get_pad_template(
                                GST_ELEMENT_GET_CLASS(link_data->tee), "src_%u");
    link_data->srcpad = gst_element_request_pad(link_data->tee, templ, pad_name,
                        NULL);

    if (link_data->srcpad == NULL) {
        LOG_ERROR("Failed to request srcpad named \'%s\' from element \'%s\'", pad_name,
                  tee_name);
        custom_link_data_destroy(link_data);
        return -1;
    }

    link_data->queue = gst_bin_get_by_name(GST_BIN(mp->pipeline), queue_name);

    if (link_data->queue == NULL) {
        LOG_ERROR("Failed to get queue element named \'%s\'", tee_name);
        custom_link_data_destroy(link_data);
        return -1;
    }

    link_data->sinkpad = gst_element_get_static_pad(link_data->queue, "sink");

    if (link_data->sinkpad == NULL) {
        LOG_ERROR("Failed to get static sinkpad from element \'%s\'", queue_name);
        custom_link_data_destroy(link_data);
        return -1;
    }

    gst_pad_add_probe(link_data->srcpad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                      attach_probe_cb, link_data, (GDestroyNotify) custom_link_data_destroy);
    return 0;
}

static GstPadProbeReturn
detach_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    custom_link_data_t *link_data = (custom_link_data_t *) user_data;
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    gst_pad_unlink(link_data->srcpad, link_data->sinkpad);
    gst_element_release_request_pad(link_data->tee, link_data->srcpad);
    return GST_PAD_PROBE_REMOVE;
}

int mediapipe_detach_channel(mediapipe_t *mp, const gchar *tee_name,
                             guint8 branch_id, const char *queue_name)
{
    custom_link_data_t *link_data = g_new0(custom_link_data_t, 1);
    link_data->tee = gst_bin_get_by_name(GST_BIN(mp->pipeline), tee_name);

    if (link_data->tee == NULL) {
        LOG_ERROR("Failed to get tee element named \'%s\'", tee_name);
        g_free(link_data);
        return -1;
    }

    gchar pad_name[16];
    sprintf(pad_name, "src_%u", branch_id);
    link_data->srcpad = gst_element_get_static_pad(link_data->tee, pad_name);

    if (link_data->srcpad == NULL) {
        LOG_ERROR("Failed to get static srcpad named \'%s\' from element \'%s\'",
                  pad_name, tee_name);
        custom_link_data_destroy(link_data);
        return -1;
    }

    link_data->queue = gst_bin_get_by_name(GST_BIN(mp->pipeline), queue_name);

    if (link_data->queue == NULL) {
        LOG_ERROR("Failed to get queue element named \'%s\'", queue_name);
        custom_link_data_destroy(link_data);
        return -1;
    }

    link_data->sinkpad = gst_element_get_static_pad(link_data->queue, "sink");

    if (link_data->sinkpad == NULL) {
        LOG_ERROR("Failed to get static sinkpad from element \'%s\'", queue_name);
        custom_link_data_destroy(link_data);
        return -1;
    }

    gst_pad_add_probe(link_data->srcpad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                      detach_probe_cb, link_data, (GDestroyNotify) custom_link_data_destroy);
    return 0;
}

static void
custom_link_data_destroy(custom_link_data_t *link_data)
{
    RETURN_IF_FAIL(link_data != NULL);

    if (link_data->tee) {
        gst_object_unref(link_data->tee);
    }

    if (link_data->srcpad) {
        gst_object_unref(link_data->srcpad);
    }

    if (link_data->queue) {
        gst_object_unref(link_data->queue);
    }

    if (link_data->sinkpad) {
        gst_object_unref(link_data->sinkpad);
    }

    g_free(link_data);
}

