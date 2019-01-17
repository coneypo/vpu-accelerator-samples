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

#define GST_VIDEO_EVENT_LONG_TERM_REF_NAME "GstLongTermFrame"
static GstEvent *
gst_video_event_new_downstream_long_term_frame(
    GstClockTime timestamp,
    GstClockTime stream_time, GstClockTime running_time, gboolean all_headers,
    guint count);

static int mediapipe_set_long_term_frame(mediapipe_t *mp,
        const gchar *element_name);


static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata);

static mp_command_t  mp_longterm_commands[] = {
    {
        mp_string("longterm"),
        MP_MAIN_CONF,
        NULL,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_core_module_t  mp_longterm_module_ctx = {
    mp_string("longterm"),
    NULL,
    NULL
};

mp_module_t  mp_longterm_module = {
    MP_MODULE_V1,
    &mp_longterm_module_ctx,                /* module context */
    mp_longterm_commands,                   /* module directives */
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
    guint ret=0;
    GValueArray *pLtArray = NULL;

    if (userdata == NULL) {
        return MP_ERROR;
    }

    char *key = (char *) userdata;

    if (key[0]=='?') {
        printf(" ===== 'y' : set long term reference frame               =====\n");
        printf(" ===== 'Y' : unset long term reference frame             =====\n");
        return MP_IGNORE;
    }

    if (key[0] != 'y' && key[0] != 'Y') {
        return MP_IGNORE;
    }

    //insert long term frame
    if (key[0] == 'y') {
        if (0 == mediapipe_set_long_term_frame(mp, "src")) {
            printf("set long term reference frame!\n");
            return MP_OK;
        } else {
            printf("set long term reference frame failed\n");
            return MP_ERROR;
        }
    } else {
        //get the frame id need to be removed
        guint str_len = strlen(key);
        guint  unset_index = 0;

        if (3 <= str_len && str_len <= 4)  {
            char *str_num = key + 1;
            unset_index = atoi(str_num);
        }

        // get recenty setted LongTermid;
        guint t_last_lt_value = 0;
        MEDIAPIPE_GET_PROPERTY(ret, mp, "enc0", "LastLongTermId", &t_last_lt_value,
                               NULL);

        if (0 == ret) {
            printf("get last set long term reference frame,frame id:%d\n", t_last_lt_value);
        } else {
            printf("get last set long term reference frame id failed\n");
        }

        //remove the frame  id
        guint t_rm_lt_frame = 0;
        MEDIAPIPE_GET_PROPERTY(ret, mp, "enc0", "LongTermList", &pLtArray, NULL);

        if (pLtArray != NULL && pLtArray->n_values > 0) {
            //remove the selet long_term  referencd frame id
            if (unset_index > 0 && unset_index <= pLtArray->n_values) {
                t_rm_lt_frame = g_value_get_uint(
                                    g_value_array_get_nth(pLtArray, unset_index - 1));
                pLtArray = g_value_array_remove(pLtArray, unset_index - 1);
                MEDIAPIPE_SET_PROPERTY(ret, mp, "enc0", "LongTermList", pLtArray, NULL);
                printf("unset long term reference frame,frame id:%d\n",
                       t_rm_lt_frame);
            } else {
                printf("command is not right \n");
            }

            if (pLtArray->n_values == 0) {
                printf("long term reference frame num is 0 now\n");
            } else {
                guint t_value = 0;
                printf("long term reference frame id list:");

                for (guint i = 0; i < pLtArray->n_values; i++) {
                    t_value = g_value_get_uint(g_value_array_get_nth(pLtArray, i));
                    printf("%d)%d ", i + 1, t_value);
                }

                printf("\n");

                if (1 == pLtArray->n_values) {
                    printf("you can use Y1 to unset the long term\n");
                } else {
                    printf("you can use Y1 to Y%d unset the long term\n",
                           pLtArray->n_values);
                }
            }
        } else {
            printf("long term reference frame num is 0 now \n");
        }

        if (pLtArray != NULL) {
            g_value_array_free(pLtArray);
        }
    }

    return MP_OK;
}

/**
    gst_video_event_new_downstream_long_term_frame:
    @timestamp: the timestamp of the buffer that starts long_term_frame
    @stream_time: the stream_time of the buffer that starts a long_term_frame
    @running_time: the running_time of the buffer that starts a long_term_frame
    @all_headers: %TRUE to produce headers when starting a new long_term_frame
    @count: integer that can be used to number key units

    it's a simulate of function gst_video_event_new_downstream_force_key_unit().
    The source codes of gst_video_event_new_downstream_force_key_unit at
    /gst-plugins-base/gst-libs/gst/video/video-event.c

    Creates a new downstream long_term_frame. A downstream long term frame
    event can be sent down the pipeline to request downstream elements to produce
    long term frame. A downstream long term frame event must also be sent when handling
    an upstream long term frame event to notify downstream that the latter has been
    handled.

    To parse an event created by gst_video_event_new_downstream_long_term_frame() use
    gst_video_event_parse_downstream_long_term_frame().

    Returns: The new GstEvent
*/
static GstEvent *
gst_video_event_new_downstream_long_term_frame(GstClockTime timestamp,
        GstClockTime stream_time,
        GstClockTime running_time,
        gboolean all_headers,
        guint count)
{
    GstEvent *long_term_frame_event;
    GstStructure *s;
    s = gst_structure_new(GST_VIDEO_EVENT_LONG_TERM_REF_NAME,
                          "timestamp", G_TYPE_UINT64, timestamp,
                          "stream-time", G_TYPE_UINT64, stream_time,
                          "running-time", G_TYPE_UINT64, running_time,
                          "all-headers", G_TYPE_BOOLEAN, all_headers,
                          "count", G_TYPE_UINT, count, NULL);
    long_term_frame_event =
        gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, s);
    return long_term_frame_event;
}

static int
mediapipe_set_long_term_frame(mediapipe_t *mp, const gchar *element_name)
{
    GstElement *element =
        gst_bin_get_by_name(GST_BIN((mp)->pipeline), (element_name));
    int ret;

    if (NULL != element) {
        GstEvent *ev = gst_video_event_new_downstream_long_term_frame(
                           GST_CLOCK_TIME_NONE,
                           GST_CLOCK_TIME_NONE,
                           GST_CLOCK_TIME_NONE,
                           TRUE,
                           1);
        GstPad *pad = gst_element_get_static_pad(element, "src");
        gst_pad_push_event(pad, ev);
        gst_object_unref(pad);
        gst_object_unref(element);
        ret = 0;
    } else {
        printf("### Can not find element '%s' ###\n", element_name);
        ret = -1;
    }

    return ret;
}

