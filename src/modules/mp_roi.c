/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mediapipe_com.h"
#include "mp_roimeta.h"

typedef struct {
    GList *roi_list;
    gint roi_disable;
} roi_ctx_t;

static GList *
json_parse_roi_info(struct json_object *root);

static GstVideoROI *
create_roi_meta(struct json_object *object);

static char *
mp_roi_block(mediapipe_t *mp, mp_command_t *cmd);

static mp_int_t
init_callback(mediapipe_t *mp);

static gboolean
enc0_sink_callback(mediapipe_t *mp, GstBuffer *buf, guint8 *data, gsize size,
                   gpointer user_data);

static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata);

static void
mediapipe_add_roi_list(GstBuffer *buffer, GList *list);

static void
exit_master(void);


static mp_command_t  mp_roi_commands[] = {
    {
        mp_string("roi"),
        MP_MAIN_CONF,
        mp_roi_block,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_core_module_t  mp_roi_module_ctx = {
    mp_string("roi"),
    NULL,
    NULL
};

static roi_ctx_t  ctx = {
    NULL,
    0
};

mp_module_t  mp_roi_module = {
    MP_MODULE_V1,
    &mp_roi_module_ctx,                /* module context */
    mp_roi_commands,                   /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    keyshot_process,                    /* keyshot_process*/
    NULL,                               /* message_process */
    init_callback,                      /* init_callback */
    NULL,                               /* netcommand_process */
    exit_master,                               /* exit master */
    MP_MODULE_V1_PADDING
};

static GstVideoROI *
create_roi_meta(struct json_object *object)
{
    gint x, y, width, height, value;
    RETURN_VAL_IF_FAIL(json_get_int(object, "x", &x), NULL);
    RETURN_VAL_IF_FAIL(json_get_int(object, "y", &y), NULL);
    RETURN_VAL_IF_FAIL(json_get_int(object, "width", &width), NULL);
    RETURN_VAL_IF_FAIL(json_get_int(object, "height", &height), NULL);
    RETURN_VAL_IF_FAIL(json_get_int(object, "value", &value), NULL);
    RETURN_VAL_IF_FAIL(width > 0 && height > 0, NULL);
    return gst_video_roi_create(x, y, width, height, value);
}

static GList *
json_parse_roi_info(struct json_object *root)
{
    GList *list = NULL;
    struct json_object *parent, *object;
    RETURN_VAL_IF_FAIL(root != NULL, NULL);
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(root, "roi", &parent), NULL);
    guint num_rois = json_object_array_length(parent);

    for (guint i = 0; i < num_rois; ++i) {
        object = json_object_array_get_idx(parent, i);
        GstVideoROI *meta = create_roi_meta(object);

        if (meta) {
            list = g_list_append(list, meta);
        }
    }

    return list;
}

static char *
mp_roi_block(mediapipe_t *mp, mp_command_t *cmd)
{
    ctx.roi_list = json_parse_roi_info(mp->config);
    return MP_CONF_OK;
}

static mp_int_t
init_callback(mediapipe_t *mp)
{
    mediapipe_set_user_callback(mp, "enc0", "sink", enc0_sink_callback, NULL);
}


//get information from user_data, modify roi
static gboolean
enc0_sink_callback(mediapipe_t *mp, GstBuffer *buf, guint8 *data, gsize size,
                   gpointer user_data)
{
    if (0 == g_atomic_int_get(&ctx.roi_disable)) {
        mediapipe_add_roi_list(buf, ctx.roi_list);
    }

    return TRUE;
}

static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata)
{
    if (userdata == NULL) {
        return MP_ERROR;
    }

    char *key = (char *) userdata;

    if (key[0]=='?') {
        printf(" ===== 'x' : turn on/off roi                             =====\n");
        return MP_IGNORE;
    }

    if (key[0] != 'x') {
        return MP_IGNORE;
    }

    static int roi_disable = 0;
    roi_disable = !roi_disable;
    g_atomic_int_set(&ctx.roi_disable, roi_disable ? 1 : 0);
    printf("turn roi %s\n", roi_disable ? "off" : "on");
    return MP_OK;
}

/**
    @brief Add ROI list to encode element.

    @param buf This is the buffer that will be passed to mix element.
    @param list The list of ROI area.
*/
static void
mediapipe_add_roi_list(GstBuffer *buffer, GList *list)
{
    g_assert(buffer && gst_buffer_is_writable(buffer));
    gst_buffer_add_video_roi_meta(buffer, list, "oclroimetainfo");
}

static void exit_master(void)
{
    g_list_free_full(ctx.roi_list, (GDestroyNotify)g_free);
}



