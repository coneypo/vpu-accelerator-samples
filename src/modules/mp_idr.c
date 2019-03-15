/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mediapipe_com.h"

static int mediapipe_set_key_frame(mediapipe_t *mp, const gchar *element_name);

static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata);

static mp_command_t  mp_idr_commands[] = {
    {
        mp_string("idr"),
        MP_MAIN_CONF,
        NULL,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_module_ctx_t  mp_idr_module_ctx = {
    mp_string("idr"),
    NULL,
    NULL,
    NULL
};


mp_module_t  mp_idr_module = {
    MP_MODULE_V1,
    &mp_idr_module_ctx,                /* module context */
    mp_idr_commands,                   /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    keyshot_process,                    /* keyshot_process*/
    NULL,                               /* message_process */
    NULL,                      /* init_callback */
    NULL,                               /* netcommand_process */
    NULL,                               /* exit master */
    MP_MODULE_V1_PADDING
};

static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata)
{
    if (userdata == NULL) {
        return MP_ERROR;
    }

    char *key = (char *) userdata;

    if (key[0]=='?') {
        printf(" ===== 'i' : force IDR                                   =====\n");
        return MP_IGNORE;
    }

    if (key[0] != 'i') {
        return MP_IGNORE;
    }

    if (0 == mediapipe_set_key_frame(mp, "src")) {
        printf("Force IDR!\n");
        return MP_OK;
    } else {
        printf("Force IDR failed\n");
        return MP_ERROR;
    }
}

/**
    @brief Send en event to force IDR.

    @param mp Pointer to mediapipe.
    @param element_name The name of encoder element.

    @retval 0: Success
    @retval -1: Fail
*/
int
mediapipe_set_key_frame(mediapipe_t *mp, const gchar *element_name)
{
    GstElement *elem = gst_bin_get_by_name(GST_BIN((mp)->pipeline), (element_name));

    if (!elem) {
        g_print("### Can not find element '%s' ###\n", element_name);
        return -1;
    }

    GstEvent *event = gst_video_event_new_downstream_force_key_unit(
                          GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, TRUE, 1);
    GstPad *pad = gst_element_get_static_pad(elem, "src");
    gst_pad_push_event(pad, event);
    gst_object_unref(pad);
    gst_object_unref(elem);
    return 0;
}

