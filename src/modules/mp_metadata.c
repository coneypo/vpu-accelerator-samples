/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "hddl_mediapipe.h"
#include "process_command.h"


typedef struct {
    GHashTable *msg_pro_fun_hst;
    mediapipe_hddl_t *hp;
} metadata_ctx;

static metadata_ctx ctx = {0};

static char *
mp_metadata_block(mediapipe_t *mp, mp_command_t *cmd);

static void
exit_master(void);

static mp_command_t  mp_metadata_commands[] = {
    {
        mp_string("metadata"),
        MP_MAIN_CONF,
        mp_metadata_block,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_core_module_t  mp_metadata_module_ctx = {
    mp_string("metadata"),
    NULL,
    NULL
};

mp_module_t  mp_metadata_module = {
    MP_MODULE_V1,
    &mp_metadata_module_ctx,                /* module context */
    mp_metadata_commands,                   /* module directives */
    MP_CORE_MODULE,                         /* module type */
    NULL,                                   /* init master */
    NULL,                                   /* init module */
    NULL,                                   /* keyshot_process*/
    NULL,                                   /* message_process */
    NULL,                                   /* init_callback */
    NULL,                                   /* netcommand_process */
    exit_master,                            /* exit master */
    MP_MODULE_V1_PADDING
};

static mp_int_t
parse_message_and_add_buffer(const char *message_name,
                            const char *subscribe_name, GstMessage *message)
{
    GstClockTime msg_pts;
    const GstStructure *root, *boxed;
    const GValue *vlist, *item;
    guint nsize, i;
    guint x, y, width, height;
    gint label_id;
    const GValue *confidence;
    uint32_t msg_type;

    // Parse messages from openvino
    root = gst_message_get_structure(message);
    gst_structure_get_clock_time(root, "timestamp", &msg_pts);
    float ts = msg_pts/1000000000.0;
    vlist = gst_structure_get_value(root, message_name);
    nsize = gst_value_list_get_size(vlist);
    for (i = 0; i < nsize; ++i) {
        item = gst_value_list_get_value(vlist, i);
        boxed = (GstStructure *) g_value_get_boxed(item);
        if (gst_structure_get_uint(boxed, "x", &x)
            && gst_structure_get_uint(boxed, "y", &y)
            && gst_structure_get_uint(boxed, "width", &width)
            && gst_structure_get_uint(boxed, "height", &height)
            && gst_structure_get_int(boxed, "label_id", &label_id)
            && (confidence = gst_structure_get_value(boxed, "confidence"))) {
                GString *metadata = g_string_new(NULL);
                g_string_append_printf(metadata, "rect = (%d, %d, %d, %d),", x, y, width, height);
                g_string_append_printf(metadata,"label_id = %d, confidence = %f,", label_id, g_value_get_float(confidence));
                g_string_append_printf(metadata, "ts = %f",ts);
                msg_type = eCommand_Metadata; 

                // Add metadata to client send buffer
                LOG_DEBUG("metadata = %s, len = %ld, ts = %f\n", metadata->str, metadata->len, ts);
                usclient_msg_to_send_buffer(ctx.hp->client, metadata->str, metadata->len, msg_type);
                g_string_free(metadata, TRUE);
            }
    }
    return MP_OK;
}

static mp_int_t
process_vehicle_detection_message(const char *message_name,
                                  const char *subscribe_name, GstMessage *message)
{
    return parse_message_and_add_buffer(message_name, subscribe_name,
                                       message);
}

static mp_int_t
process_crossroad_detection_message(const char *message_name,
                                  const char *subscribe_name, GstMessage *message)
{
    return parse_message_and_add_buffer(message_name, subscribe_name,
                                       message);
}

static mp_int_t
process_barrier_detection_message(const char *message_name,
                                  const char *subscribe_name, GstMessage *message)
{
    return parse_message_and_add_buffer(message_name, subscribe_name,
                                       message);
}

static gboolean
json_analyse_and_post_message(mediapipe_t *mp, const gchar *elem_name)
{
    mediapipe_hddl_t *hp = (mediapipe_hddl_t*) mp;
    if (hp->client == NULL) {
        LOG_DEBUG("client service not available, skip send metadata.");
        return FALSE;
    }
    ctx.hp = hp; 

    //find metadata json object
    struct json_object *root;
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(mp->config, elem_name,
                       &root), FALSE);

    //init message process fun hash table
    if (NULL == ctx.msg_pro_fun_hst) {
        ctx.msg_pro_fun_hst = g_hash_table_new_full(g_str_hash, g_str_equal,
                                   (GDestroyNotify)g_free, NULL);
        g_hash_table_insert(ctx.msg_pro_fun_hst, g_strdup("vehicle_detection"),
                            (gpointer) process_vehicle_detection_message);
        g_hash_table_insert(ctx.msg_pro_fun_hst, g_strdup("crossroad_detection"),
                            (gpointer) process_crossroad_detection_message);
        g_hash_table_insert(ctx.msg_pro_fun_hst, g_strdup("barrier_detection"),
                            (gpointer) process_barrier_detection_message);
    }

    //analyze config , post message
    struct json_object *array = NULL;
    struct json_object *message_obj = NULL;
    const char *message_name;
    const char *subscriber_name = "send_metadata";

    if (json_object_object_get_ex(root, "subscribe_message", &array)) {
        guint  num_values = json_object_array_length(array);
        for (guint i = 0; i < num_values; i++) {
            message_obj = json_object_array_get_idx(array, i);
            message_name = json_object_get_string(message_obj);
            gpointer fun = g_hash_table_lookup(ctx.msg_pro_fun_hst, message_name);
            if (NULL != fun) {
                GstMessage *m;
                GstStructure *s;
                GstBus *bus = gst_element_get_bus(mp->pipeline);
                s = gst_structure_new("subscribe_message",
                                      "message_name", G_TYPE_STRING, message_name,
                                      "subscriber_name", G_TYPE_STRING, subscriber_name,
                                      "message_process_fun", G_TYPE_POINTER, fun,
                                      NULL);
                m = gst_message_new_application(NULL, s);
                LOG_DEBUG("send metadata: message_name: %s", message_name);
                gst_bus_post(bus, m);
                gst_object_unref(bus);
            }
        }
        return TRUE;
    } else {
        return FALSE;
    }
}

static char *
mp_metadata_block(mediapipe_t *mp, mp_command_t *cmd)
{
    if (mp->config == NULL) {
        return (char *) MP_CONF_ERROR;
    }

    json_object_object_foreach(mp->config, key, val) {
        if (NULL != strstr(key, "metadata")) {
            gboolean ret = json_analyse_and_post_message(mp, key);
            if(ret == FALSE) {
                LOG_ERROR("failed to post metadata bus message\n");
            }
        }
    }
    return MP_CONF_OK;
}

static void
exit_master(void)
{
    g_hash_table_unref(ctx.msg_pro_fun_hst);
}


