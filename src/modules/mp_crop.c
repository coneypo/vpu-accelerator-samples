/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mediapipe_com.h"
#include "gstocl/oclcommon.h"


static mp_int_t
init_callback(mediapipe_t *mp);

static gboolean
scale2_sink_callback(mediapipe_t *mp, GstBuffer *buf, guint8 *data, gsize size,
                     gpointer user_data);

void
mediapipe_set_crop(GstBuffer *buffer, guint16 x, guint16 y, guint16 width,
                   guint16 height);


static mp_command_t  mp_crop_commands[] = {
    {
        mp_string("crop"),
        MP_MAIN_CONF,
        NULL,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_core_module_t  mp_crop_module_ctx = {
    mp_string("crop"),
    NULL,
    NULL
};

mp_module_t  mp_crop_module = {
    MP_MODULE_V1,
    &mp_crop_module_ctx,                /* module context */
    mp_crop_commands,                   /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    NULL,                    /* keyshot_process*/
    NULL,                               /* message_process */
    init_callback,                      /* init_callback */
    NULL,                               /* netcommand_process */
    NULL,                               /* exit master */
    MP_MODULE_V1_PADDING
};

static mp_int_t
init_callback(mediapipe_t *mp)
{
    mediapipe_set_user_callback(mp, "scale2", "sink", scale2_sink_callback, NULL);
    return MP_OK;
}

static gboolean
scale2_sink_callback(mediapipe_t *mp, GstBuffer *buf, guint8 *data, gsize size,
                     gpointer user_data)
{
    static unsigned int x = 0, y = 0;
    mediapipe_set_crop(buf, x, y, 800, 600);

    if (++x >= 1023) {
        x = 0;
    }

    if (++y >= 399) {
        y = 0;
    }

    return TRUE;
}

/**
 * @brief Set the cropping area for scale element.
 *
 * @param buf This is the buffer that will be passed to scale element.
 * @param x The x coordinate of left-top corner of this crop area.
 * @param y The y coordinate of left-top corner of this crop area.
 * @param width The width of this crop area.
 * @param height The height of this crop area.
 */
void
mediapipe_set_crop(GstBuffer *buffer, guint16 x, guint16 y, guint16 width,
                   guint16 height)
{
    g_assert(buffer && gst_buffer_is_writable(buffer));
#if VER >= VER_ALPHA
    gst_buffer_add_ocl_scale_meta(buffer, x, y, width, height);
#endif
}

