/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */
#include "mediapipe.h"
#include "mediapipe_com.h"
#include "utils/packet_struct_v2.h"

#include <gst/app/app.h>
#include <vector>

typedef struct {
    GAsyncQueue* meta_queue;
} stream_ctx_t;

static mp_int_t
init_callback(mediapipe_t* mp);

static void*
create_ctx(mediapipe_t* mp);

static void
destroy_ctx(void* _ctx);

static char* load_config(mediapipe_t* mp, mp_command_t* cmd);

static mp_command_t mp_metaparser_commands[] = {
    {
        mp_string("metaparser"),
        MP_MAIN_CONF,
        load_config,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_module_ctx_t
    mp_metaparser_module_ctx
    = {
        mp_string("metaparser"),
        create_ctx,
        nullptr,
        destroy_ctx
      };

mp_module_t
    mp_metaparser_module
    = {
        MP_MODULE_V1,
        &mp_metaparser_module_ctx, /* module context */
        mp_metaparser_commands, /* module directives */
        MP_CORE_MODULE, /* module type */
        nullptr, /* init master */
        nullptr, /* init module */
        nullptr, /* keyshot_process*/
        nullptr, /* message_process */
        init_callback, /* init_callback */
        nullptr, /* netcommand_process */
        nullptr, /* exit master */
        MP_MODULE_V1_PADDING
      };


static gboolean
myconvert_src_callback(mediapipe_t* mp, GstBuffer* buffer, guint8* data, gsize size, gpointer user_data)
{
    auto ctx = (stream_ctx_t*)user_data;

    GstStructure* walk = (GstStructure*)g_async_queue_try_pop(ctx->meta_queue);
    if (!walk) {
        return TRUE;
    }

    const GValue* vlist = gst_structure_get_value(walk, "meta_info");
    if (!vlist) {
        LOG_WARNING("unexpected GstStructure object");
        gst_structure_free(walk);
        return TRUE;
    }

    guint top, left, width, height;
    guint nsize = gst_value_list_get_size(vlist);

    for (guint i = 0; i < nsize; i++) {
        const GValue* item = gst_value_list_get_value(vlist, i);
        GstStructure* boxed = (GstStructure*)g_value_get_boxed(item);
        gst_structure_get_uint(boxed, "left", &left);
        gst_structure_get_uint(boxed, "top", &top);
        gst_structure_get_uint(boxed, "width", &width);
        gst_structure_get_uint(boxed, "height", &height);
        GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(
                buffer, "label", left, top, width, height);
        gst_video_region_of_interest_meta_add_param(meta, gst_structure_copy(boxed));
    }

    gst_structure_free(walk);

    return TRUE;
}

static void*
create_ctx(mediapipe_t* mp)
{
    g_assert(mp != NULL);
    stream_ctx_t* ctx = g_new0(stream_ctx_t, 1);
    ctx->meta_queue = g_async_queue_new_full((GDestroyNotify)gst_structure_free);

    return ctx;
}

static void
destroy_ctx(void* _ctx)
{
    stream_ctx_t* ctx = (stream_ctx_t*)_ctx;

    if (ctx->meta_queue) {
        g_async_queue_unref(ctx->meta_queue);
        ctx->meta_queue = NULL;
    }

    g_free(_ctx);
}

static gboolean
xlinksrc_src_callback(mediapipe_t* mp, GstBuffer* buffer, guint8* data, gsize size, gpointer user_data)
{
    auto ctx = (stream_ctx_t*)user_data;
    GValue objectlist = { 0 };
    g_value_init(&objectlist, GST_TYPE_LIST);

    GstMapInfo info;
    if (!gst_buffer_map(buffer, &info, GST_MAP_READ)) {
        LOG_ERROR("xlinkreader: gst_buffer_map failed");
        return FALSE;
    }

    auto pData = info.data;
    auto header = reinterpret_cast<Header*>(pData);
    auto meta = reinterpret_cast<Meta*>(pData + sizeof(Header));
    auto roi = reinterpret_cast<ROI*>(pData + sizeof(Header) + sizeof(Meta));

    auto frameId = meta->frame_number;
    auto headerSize = header->meta_size + sizeof(header) + (meta->num_rois * sizeof(ROI));

    for (int i = 0; i < meta->num_rois; i++) {
        //GstVideoRegionOfInterestMeta* meta = gst_buffer_add_video_region_of_interest_meta(
        //   buffer, "label", border[i].left, border[i].top, border[i].width, border[i].height);
        GValue tmp_value = { 0 };
        g_value_init(&tmp_value, GST_TYPE_STRUCTURE);

        GstStructure* s = gst_structure_new(
            "detection",
            "magic", G_TYPE_UINT, header->magic,
            "version", G_TYPE_UINT, header->version,
            "metaversion", G_TYPE_UINT, meta->version,
            "stream_id", G_TYPE_UINT, meta->stream_id,
            "frame_number", G_TYPE_UINT, meta->frame_number,
            "num_rois", G_TYPE_UINT, meta->num_rois,
            "classification_index", G_TYPE_UINT, roi[i].classification_index,
            "reserved", G_TYPE_UINT, roi[i].reserved,
            "object_index", G_TYPE_UINT, roi[i].object_index,
            "left", G_TYPE_UINT, roi[i].left,
            "top", G_TYPE_UINT, roi[i].top,
            "width", G_TYPE_UINT, roi[i].width,
            "height", G_TYPE_UINT, roi[i].height, NULL);

        g_value_take_boxed(&tmp_value, s);
        gst_value_list_append_value(&objectlist, &tmp_value);
        g_value_unset(&tmp_value);
    }

    GstStructure* s = gst_structure_new_empty("detection");
    gst_structure_set_value(s, "meta_info", &objectlist);
    g_value_unset(&objectlist);

    g_async_queue_push(ctx->meta_queue, s);

    gst_buffer_unmap(buffer, &info);

    gst_buffer_resize(buffer, headerSize, -1);

    return TRUE;
}

static mp_int_t init_callback(mediapipe_t* mp)
{
    auto ctx = mp_modules_find_moudle_ctx(mp, "metaparser");
    mediapipe_set_user_callback(mp, "src", "src", xlinksrc_src_callback, ctx);
    mediapipe_set_user_callback(mp, "myconvert", "src", myconvert_src_callback, ctx);
    return MP_OK;
}

static char* load_config(mediapipe_t* mp, mp_command_t* cmd)
{
    int channelId = -1;
    if (!mediapipe_get_channelId(mp, "src", &channelId)) {
        return MP_CONF_OK;
    }

    GstElement* xlinksrc = gst_bin_get_by_name(GST_BIN(mp->pipeline), "src");
    if (!xlinksrc) {
        LOG_WARNING("cannot find element named \"src\".");
        return MP_CONF_OK;
    }

    if (g_object_class_find_property(G_OBJECT_GET_CLASS(xlinksrc), "channel")) {
        g_object_set(xlinksrc, "channel", channelId, NULL);
        LOG_INFO("set property \"channel\" as (%d) on element named \"src\".", channelId);
    } else {
        LOG_WARNING("cannot find property \"channel\" on element named \"src\".");
    }

    if (g_object_class_find_property(G_OBJECT_GET_CLASS(xlinksrc), "init-xlink")) {
        g_object_set(xlinksrc, "init-xlink", TRUE, NULL);
    } else {
        LOG_WARNING("cannot find property \"init-xlink\" on element named \"src\".");
    }

    return MP_CONF_OK;
}
