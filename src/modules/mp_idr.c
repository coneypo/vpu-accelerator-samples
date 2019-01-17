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

static mp_core_module_t  mp_idr_module_ctx = {
    mp_string("idr"),
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

