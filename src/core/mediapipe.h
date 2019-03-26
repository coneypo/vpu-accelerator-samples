/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
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

typedef void(*message_callback_t)(mediapipe_t* mp, GstMessage *msg);

struct mediapipe_s {
    MEDIAPIPE_STATE     state;
    GMainLoop           *loop;
    GstElement          *pipeline;
    guint               bus_watch_id;
    GstRTSPServer       *rtsp_server;
    GList               *probe_data_list;
    struct json_object  *config;
    mp_module_t         **modules;
    mp_uint_t           modules_n;
    void                **module_ctx;

    int                 xlink_channel_id;
    message_callback_t  message_callback;
    void                *private_data;
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

gboolean
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
