/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mediapipe_com.h"
#include "mp_seimeta.h"

typedef struct {
    GMutex add_sei_lock;
    gboolean add_sei_enable;
} sei_ctx_t;

static char *
mp_sei_block(mediapipe_t *mp, mp_command_t *cmd);

static void mediapipe_add_sei(GstBuffer *buf, const guint8 *sei_data,
                              guint32 sei_len);

static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata);

static gboolean
enc0_sink_callback(mediapipe_t *mp, GstBuffer *buf, guint8 *data, gsize size,
                   gpointer user_data);

static mp_int_t
init_callback(mediapipe_t *mp);




static mp_command_t  mp_sei_commands[] = {
    {
        mp_string("sei"),
        MP_MAIN_CONF,
        mp_sei_block,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_module_ctx_t  mp_sei_module_ctx = {
    mp_string("sei"),
    NULL,
    NULL,
    NULL
};

static sei_ctx_t  ctx = { 0 };

mp_module_t  mp_sei_module = {
    MP_MODULE_V1,
    &mp_sei_module_ctx,                /* module context */
    mp_sei_commands,                   /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    keyshot_process,                    /* keyshot_process*/
    NULL,                               /* message_process */
    init_callback,                               /* init_callback */
    NULL,                               /* netcommand_process */
    NULL,                        /* exit master */
    MP_MODULE_V1_PADDING
};

static char *
mp_sei_block(mediapipe_t *mp, mp_command_t *cmd)
{
    g_mutex_init(&ctx.add_sei_lock);
    ctx.add_sei_enable=FALSE;
    return MP_CONF_OK;
}


static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata)
{
    mp_int_t ret = MP_OK;

    if (userdata == NULL) {
        return MP_ERROR;
    }

    char *key = (char *) userdata;

    if (key[0]=='?') {
        printf(" ===== 'e' : insert a test SEI                           =====\n");
        return MP_IGNORE;
    }

    if (key[0] != 'e') {
        return MP_IGNORE;
    }

    g_mutex_lock(&ctx.add_sei_lock);
    ctx.add_sei_enable = TRUE;
    g_mutex_unlock(&ctx.add_sei_lock);
    return ret;
}


/**
    @brief Add SEI data to the encode element.

    @param buf This is the buffer that will be passed to encode element.
    @param sei_data Pointer to sei_data. Emulation prevention byte will be insert to sei_data.
    @param data_size Size of sei_data.
*/
void
mediapipe_add_sei(GstBuffer *buffer, const guint8 *sei_data, guint32 data_size)
{
    g_assert(buffer && gst_buffer_is_writable(buffer));
    g_assert(sei_data);
    gst_buffer_add_sei_meta(buffer, sei_data, data_size);
}

//get information from user_data, modify roi
static gboolean
enc0_sink_callback(mediapipe_t *mp, GstBuffer *buf, guint8 *data, gsize size,
                   gpointer user_data)
{
    const guint8 test_sei [] = {
        0x05,
        0xdc, 0x45, 0xe9, 0xbd, 0xe6, 0xd9, 0x48, 0xb7,
        0x96, 0x2c, 0xd8, 0x20, 0xd9, 0x23, 0xee, 0xef,
        'I', ' ', 'a', 'm', ' ', 'S', 'E', 'I'
    };
    g_mutex_lock(&ctx.add_sei_lock);

    if (ctx.add_sei_enable) {
        mediapipe_add_sei(buf, test_sei, sizeof(test_sei));
        ctx.add_sei_enable = FALSE;
    }

    g_mutex_unlock(&ctx.add_sei_lock);
    return TRUE;
}

static mp_int_t
init_callback(mediapipe_t *mp)
{
    mediapipe_set_user_callback(mp, "enc0", "sink", enc0_sink_callback, NULL);
    return MP_OK;
}

