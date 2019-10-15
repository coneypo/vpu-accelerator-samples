/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mediapipe_com.h"

#define MAX_BUF_SIZE 512
#define QUEUE_CAPACITY 10
#define SNAPSHOT_DESC "appsrc name=source ! mfxjpegenc name=encoder ! fakesink"

typedef struct {
    mediapipe_branch_t  branch;
    GMutex jpeg_capture_lock;
    gboolean jpeg_capture_enable;
} snap_ctx_t;

static snap_ctx_t ctx;

static
gboolean branch_init(mediapipe_branch_t *);

static
mp_int_t init_module(mediapipe_t *mp);

static gboolean
snapshot_branch_config(mediapipe_branch_t *branch);

static void exit_master(void);

static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata);

static mp_int_t
init_callback(mediapipe_t *mp);



/* static mp_command_t  mp_snapshot_commands[] = { */
/*     { mp_string("snapshot"), */
/*       MP_MAIN_CONF, */
/*       NULL, */
/*       0, */
/*       0, */
/*       NULL }, */
/*       mp_null_command */
/* }; */

static mp_module_ctx_t  mp_snapshot_module_ctx = {
    mp_string("snapshot"),
    NULL,
    NULL,
    NULL
};

mp_module_t  mp_snapshot_module = {
    MP_MODULE_V1,
    &mp_snapshot_module_ctx,                /* module context */
    NULL,                   /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                               /* init master */
    init_module,                               /* init module */
    keyshot_process,                    /* keyshot_process*/
    NULL,                               /* message_process */
    init_callback,                      /* init_callback */
    NULL,                               /* netcommand_process */
    exit_master,                               /* exit master */
    MP_MODULE_V1_PADDING
};

static GstPadProbeReturn
save_as_file(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    GstMapInfo map;
    gchar filename[MAX_BUF_SIZE];
    struct timespec tspec;
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);

    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        return GST_PAD_PROBE_OK;
    }

    clock_gettime(CLOCK_REALTIME, &tspec);
    struct tm *local_time = localtime(&tspec.tv_sec);

    if (!local_time) {
        return GST_PAD_PROBE_OK;
    }

    snprintf(filename, MAX_BUF_SIZE,
             "snapshot at %4d-%02d-%02d %02d-%02d-%02d-%ld.jpg",
             local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
             local_time->tm_hour, local_time->tm_min, local_time->tm_sec,
             tspec.tv_nsec % 1000);
    FILE *fp = fopen(filename, "wb");

    if (fp) {
        fwrite(map.data, 1, map.size, fp);
        fclose(fp);
    }

    gst_buffer_unmap(buffer, &map);
    return GST_PAD_PROBE_OK;
}

static gboolean
snapshot_branch_config(mediapipe_branch_t *branch)
{
    gchar desc[MAX_BUF_SIZE];
    guint width = branch->input_width;
    guint height = branch->input_height;

    if (width <= 0 || height <= 0 || branch->pipeline == NULL) {
        return FALSE;
    }

    snprintf(desc, MAX_BUF_SIZE,
             "video/x-raw,format=NV12,width=%u,height=%u,framerate=30/1", width, height);
    GstCaps *caps = gst_caps_from_string(desc);
    guint max_bytes = width * height * QUEUE_CAPACITY;
    branch->source = gst_bin_get_by_name(GST_BIN(branch->pipeline), "source");

    if (!branch->source) {
        return FALSE;
    }

    g_object_set(branch->source, "format", GST_FORMAT_TIME, "max_bytes", max_bytes,
                 "caps", caps, NULL);
    GstElement *encoder = gst_bin_get_by_name(GST_BIN(branch->pipeline), "encoder");

    if (!encoder) {
        return FALSE;
    }

    GstPad *srcpad = gst_element_get_static_pad(encoder, "src");
    gulong id = gst_pad_add_probe(srcpad, GST_PAD_PROBE_TYPE_BUFFER, save_as_file,
                                  NULL, NULL);
    gboolean success = gst_probe_list_append_new_item(branch->probe_items, srcpad,
                       id);
    gst_caps_unref(caps);
    gst_object_unref(encoder);
    return success;
}

static gboolean
mix_src_callback(mediapipe_t *mp, GstBuffer *buf, guint8 *data, gsize size,
                 gpointer user_data)
{
    g_mutex_lock(&ctx.jpeg_capture_lock);

    if (ctx.jpeg_capture_enable) {
        ctx.jpeg_capture_enable = FALSE;
        mediapipe_branch_push_buffer(&ctx.branch, buf);
    }

    g_mutex_unlock(&ctx.jpeg_capture_lock);
    return TRUE;
}

static
gboolean branch_init(mediapipe_branch_t *branch)
{
    branch->pipeline = mediapipe_branch_create_pipeline(SNAPSHOT_DESC);

    if (!snapshot_branch_config(branch)) {
        mediapipe_branch_destroy_internal(branch);
        return FALSE;
    }

    return TRUE;
}

static
mp_int_t init_module(mediapipe_t *mp)
{
    g_mutex_init(&ctx.jpeg_capture_lock);
    ctx.branch.branch_init = branch_init;
    mediapipe_setup_new_branch(mp, "mix", "src", &ctx.branch);
    return MP_OK;
}

static void exit_master(void)
{
    mediapipe_branch_destroy_internal(&ctx.branch);
    g_mutex_clear(&ctx.jpeg_capture_lock);
}

static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata)
{
    if (userdata == NULL) {
        return MP_ERROR;
    }

    char *key = (char *) userdata;

    if (key[0]=='?') {
        printf(" ===== 'c' : snapshot                                    =====\n");
        return MP_IGNORE;
    }

    if (key[0] != 'c') {
        return MP_IGNORE;
    }

    g_mutex_lock(&ctx.jpeg_capture_lock);
    ctx.jpeg_capture_enable = TRUE;
    g_mutex_unlock(&ctx.jpeg_capture_lock);
    return MP_OK;
}

static mp_int_t
init_callback(mediapipe_t *mp)
{
    mediapipe_set_user_callback(mp, "mix", "src", mix_src_callback, &ctx.branch);
    return MP_OK;
}

