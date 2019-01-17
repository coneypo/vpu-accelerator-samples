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

#ifndef __MEDIAPIPE_H__
#define __MEDIAPIPE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "mediapipe_com.h"

typedef struct probe_context_s probe_context_t;

typedef gboolean(*user_callback_t)(mediapipe_t *mp, GstBuffer *buf, guint8 *data,
                                gsize size, gpointer user_data);

struct probe_context_s {
    mediapipe_t           *mp;
    GstElement          *element;
    const char          *pad_name;

    user_callback_t     user_callback;
    gpointer            user_data;

    const char          *caps_string;
    gint                fps;
    GstElement          *src;
    GstClockTime        timestamp;
};

struct mediapipe_s {
    MEDIAPIPE_STATE     state;
    GMainLoop           *loop;
    GstElement          *pipeline;
    guint               bus_watch_id;
    GstRTSPServer       *rtsp_server;
    GList               *probe_data_list;
    struct json_object  *config;
    mp_module_t        **modules;
    mp_uint_t          modules_n;
    void                 ** **conf_ctx;
};

void
get_resolution_from_caps(GstCaps *caps, gint *width, gint *height);

mediapipe_t *
mediapipe_create(int argc, char *argv[]);


gboolean
mediapipe_init_from_string(const char *config, const char *launch, mediapipe_t *mp);

void
mediapipe_destroy(mediapipe_t *mp);

void
mediapipe_start(mediapipe_t *mp);

void
mediapipe_stop(mediapipe_t *mp);

void
mediapipe_playing(mediapipe_t *mp);

void
mediapipe_pause(mediapipe_t *mp);

int
add_probe_callback(GstPadProbeCallback probe_callback, probe_context_t *ctx);

probe_context_t *
create_callback_context(mediapipe_t *mp, const gchar *elem_name,
                        const gchar *pad_name);

int
mediapipe_set_user_callback(mediapipe_t *mp,
                            const gchar *element_name, const gchar *pad_name,
                            user_callback_t user_callback, gpointer user_data);


int
mediapipe_remove_user_callback(mediapipe_t *mp,
                            const gchar *element_name, const gchar *pad_name,
                            user_callback_t user_callback, gpointer user_data);

#define MEDIAPIPE_SET_PROPERTY(ret, mp, element_name, property_name,  ...) \
    do { \
        GstElement *element =  \
                               gst_bin_get_by_name (GST_BIN((mp)->pipeline), (element_name)); \
        if (NULL != element) { \
        GParamSpec* ele_param =  \
            g_object_class_find_property(G_OBJECT_GET_CLASS(element), (property_name)); \
        if (NULL != ele_param) { \
            g_object_set(element, property_name, __VA_ARGS__); \
            ret = 0; \
        } else { \
            LOG_WARNING ("### set propety failed Can not find property '%s' , element '%s' ###\n", (property_name) , (element_name)); \
            ret = -1; \
        } \
        gst_object_unref(element); \
        } else { \
            LOG_WARNING ("### set propety failed Can not find element '%s' ###\n", (element_name)); \
            ret = -1; \
        } \
    } while (0)

#define MEDIAPIPE_GET_PROPERTY(ret, mp, element_name, property_name,  ...) \
    do { \
        GstElement *element =  \
                               gst_bin_get_by_name (GST_BIN((mp)->pipeline), (element_name)); \
        if (NULL != element) { \
        GParamSpec* ele_param = g_object_class_find_property(G_OBJECT_GET_CLASS(element), (property_name)); \
        if (NULL != ele_param) { \
            g_object_get(element, property_name, __VA_ARGS__); \
            ret = 0; \
        } else { \
            LOG_WARNING ("### get propety failed, Can not  find property '%s' , element '%s' ###\n", (property_name) , (element_name)); \
            ret = -1; \
        } \
        gst_object_unref(element); \
        } else { \
            LOG_WARNING ("### get propety failed, Can not find element '%s' ###\n", (element_name)); \
            ret = -1; \
        } \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif
