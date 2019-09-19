/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>
#include <malloc.h>
#include <gst/gstallocator.h>
#include <gst/allocators/allocators.h>

typedef struct ctx_roi{
    GstElement * pipeline;
    GList * conf_List;
} CTX;

gboolean roi_process(CTX *ctx);

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data);

GstElement *create_pipeline(const gchar *pipeline_description);

static GstPadProbeReturn gapiosd_sink_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);

static gboolean Get_GstStructure_List(CTX *ctx);

static void Ctx_finalize(CTX *ctx);

static gboolean parse_and_print_config_list_property(GList *pList);

static gboolean print_field(GQuark field, const GValue *value, gpointer pfx);

GstElement *
create_pipeline(const gchar *pipeline_description)
{
    GError *error = NULL;
    GstElement *pipeline = gst_parse_launch(pipeline_description, &error);
    if (error) {
        g_clear_error(&error);
        return NULL;
    }
    return pipeline;
}

int main ()
{
    CTX *ctx = g_new0(CTX, 1);
    gst_init(0, NULL);
    roi_process(ctx);
    Ctx_finalize(ctx);
    return 0;
}

gboolean roi_process(CTX *ctx)
{

    GstStateChangeReturn ret =  GST_STATE_CHANGE_FAILURE;
    ctx->pipeline = create_pipeline("videotestsrc ! video/x-raw,format=BGR,width=1920,height=1080 ! gapiosd name=gapiosd0 ! videoconvert ! ximagesink ");
    if (ctx->pipeline == NULL) {
        g_print("create jpeg encode pipeline failed");
        return FALSE;
    }

    GstElement *gapiosd = gst_bin_get_by_name( GST_BIN(ctx->pipeline), "gapiosd0");
    GstPad *pad = gst_element_get_static_pad(gapiosd, "sink");
    if (gapiosd == NULL || pad == NULL) {
        g_print("can't find element gapiosd, can't init callback");
        return FALSE;
    }
    Get_GstStructure_List(ctx);
    guint callback_id = gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                            gapiosd_sink_callback, ctx,
                            NULL);
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (ctx->pipeline));
    guint bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);
    ret = gst_element_set_state(ctx->pipeline, GST_STATE_PLAYING);
    if(ret == GST_STATE_CHANGE_FAILURE) {
        g_source_remove (bus_watch_id);
        gst_object_unref(GST_OBJECT(ctx->pipeline));
        return FALSE;
    }

    g_main_loop_run(loop);

    gst_element_set_state(ctx->pipeline, GST_STATE_NULL);
    gst_pad_remove_probe(pad, callback_id);
    gst_object_unref(pad);
    g_source_remove (bus_watch_id);
    gst_object_unref(ctx->pipeline);
    g_main_loop_unref(loop);
    return TRUE;
}

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data)
{
    GMainLoop *loop = (GMainLoop *) data;
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            g_print("End of stream\n");
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_ERROR: {
            gchar *debug;
            GError *error;
            gst_message_parse_error(msg, &error, &debug);
            g_free(debug);
            g_printerr("Error: %s\n", error->message);
            g_error_free(error);
            g_main_loop_quit(loop);
            break;
        }
        default:
            break;
    }
    return TRUE;
}


static GstPadProbeReturn
gapiosd_sink_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    CTX *roi_ctx = (CTX *)user_data;
    GList *pList = NULL;
    GstElement *element = gst_bin_get_by_name (GST_BIN((roi_ctx)->pipeline), "gapiosd0");
    if (NULL != element) {
        GParamSpec* ele_param = g_object_class_find_property(G_OBJECT_GET_CLASS(element), "config-list");
        if (NULL != ele_param) {
            g_object_set(element, "config-list", roi_ctx->conf_List, NULL);
            g_object_get(element, "config-list", &pList, NULL);
            if (NULL == pList) {
                g_print("### get %s propety failed of %s###\n", "config-list" , "gapiosd0");
            } else {
                parse_and_print_config_list_property(pList);
            }
        } else {
            g_print("### set propety failed Can not find property '%s' , element '%s' ###\n", "config-list" , "gapiosd0");
        }
    } else {
        g_print ("### set propety failed Can not find element '%s' ###\n", "gapiosd0");
    }

    return GST_PAD_PROBE_OK;
}

static gboolean Get_GstStructure_List(CTX *ctx)
{
    GstStructure *text_s =
    gst_structure_new("gapiosd_meta",
                    "meta_id", G_TYPE_UINT, 0,
                    "meta_type", G_TYPE_STRING, "text",
                    "text", G_TYPE_STRING, "testGstStruct",
                    "font_type", G_TYPE_INT, 4,
                    "font_scale", G_TYPE_DOUBLE, 1.8,
                    "x", G_TYPE_INT, 100,
                    "y", G_TYPE_INT, 600,
                    "r", G_TYPE_UINT, 0,
                    "g", G_TYPE_UINT, 0,
                    "b", G_TYPE_UINT, 0,
                    "line_thick", G_TYPE_INT, 1,
                    "bottom_left_origin", G_TYPE_BOOLEAN, FALSE,
                    NULL);
    GstStructure *rect_s =
        gst_structure_new("gapiosd_meta",
                          "meta_id", G_TYPE_UINT, 1,
                          "meta_type", G_TYPE_STRING, "rect",
                          "x", G_TYPE_INT, 100,
                          "y", G_TYPE_INT, 600,
                          "width", G_TYPE_INT, 200,
                          "height", G_TYPE_INT, 200,
                          "r", G_TYPE_UINT, 0,
                          "g", G_TYPE_UINT, 0,
                          "b", G_TYPE_UINT, 0,
                          "thick", G_TYPE_INT, 2,
                          "lt", G_TYPE_INT, 8,
                          "shift", G_TYPE_INT, 0,
                          NULL);
    ctx->conf_List = g_list_append(ctx->conf_List, text_s);
    ctx->conf_List = g_list_append(ctx->conf_List, rect_s);
    return TRUE;
}


static gboolean parse_and_print_config_list_property(GList *pList)
{
    g_assert(pList != NULL);
    g_print("###########config_list property:############\n");
    GstStructure *structure = NULL;
    GList *index = pList;
    while (index) {
        structure = (GstStructure *)index->data;
        gst_structure_foreach(structure, print_field, NULL);
        g_print("\n");
        index = g_list_next(index);
    }
    return TRUE;
}
static gboolean print_field(GQuark field, const GValue *value, gpointer pfx)
{
    gchar *str = gst_value_serialize(value);
    g_print("%15s: %s\n", g_quark_to_string(field), str);
    g_free(str);
    return TRUE;
}
static void Ctx_finalize(CTX *ctx)
{
    g_list_free_full(ctx->conf_List, (GDestroyNotify)gst_structure_free);
    g_free(ctx);
}
