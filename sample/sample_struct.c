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
} ctx;

gboolean roi_process(ctx *ctx);

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data);

GstElement *create_pipeline(const gchar *pipeline_description);

static GstPadProbeReturn api2d_sink_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);

static gboolean Get_GstStructure_List(ctx *ctx);

static void Ctx_finalize(ctx *ctx);

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
    ctx *ctx = alloca(sizeof(ctx));
    gst_init(0, NULL);
    roi_process(ctx);
    Ctx_finalize(ctx);
    return 0;
}

gboolean roi_process(ctx *ctx)
{

    GstStateChangeReturn ret =  GST_STATE_CHANGE_FAILURE;
    ctx->pipeline = create_pipeline("videotestsrc ! video/x-raw,format=BGR,width=1920,height=1080 ! api2d name=api2d0 ! videoconvert ! ximagesink ");
    if (ctx->pipeline == NULL) {
        g_print("create jpeg encode pipeline failed");
        return FALSE;
    }

    GstElement *api2d = gst_bin_get_by_name( GST_BIN(ctx->pipeline), "api2d0");
    GstPad *pad = gst_element_get_static_pad(api2d, "sink");
    if (api2d == NULL || pad == NULL) {
        g_print("can't find element api2d, can't init callback");
        return FALSE;
    }
    Get_GstStructure_List(ctx);
    guint callback_id = gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                            api2d_sink_callback, ctx,
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
api2d_sink_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    ctx* roi_ctx = (ctx *)user_data;
    GstElement *element = gst_bin_get_by_name (GST_BIN((roi_ctx)->pipeline), "api2d0");
    if (NULL != element) {
        GParamSpec* ele_param = g_object_class_find_property(G_OBJECT_GET_CLASS(element), "config-list");
        if (NULL != ele_param) {
            g_object_set(element, "config-list", roi_ctx->conf_List, NULL);
        } else {
            g_print("### set propety failed Can not find property '%s' , element '%s' ###\n", "config-list" , "api2d0");
        }
    } else {
        g_print ("### set propety failed Can not find element '%s' ###\n", "api2d0");
    }

    return GST_PAD_PROBE_OK;
}

static gboolean Get_GstStructure_List(ctx *ctx)
{
    GstStructure *text_s =
    gst_structure_new("textObject",
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
    ctx->conf_List = g_list_append(ctx->conf_List, text_s);
    return TRUE;
}


static void Ctx_finalize(ctx *ctx)
{
    g_object_unref(ctx->pipeline);
    g_list_free_full(ctx->conf_List, (GDestroyNotify)gst_structure_free);
    g_free(ctx);
}