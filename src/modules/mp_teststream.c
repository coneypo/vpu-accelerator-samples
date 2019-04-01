/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */
#include "mediapipe_com.h"
#include <gst/app/app.h>
#include <vector>

typedef struct {
    GQueue* meta_queue;
} stream_ctx_t;

typedef struct {
    guint8 magic;
    guint8 version;
    guint16 meta_size;
    guint32 package_size;
} Header;

typedef struct {
    guint8 version;
    guint8 packet_type;
    guint8 stream_id;
    guint8 of_objects;
    guint32 frame_number;
} Metadata;

#pragma pack(push)
#pragma pack(1)
typedef struct {
    unsigned u : 24;
} Position;
#pragma pack(pop)

typedef struct {
    guint8 reserved;
    guint8 object_id;
    guint16 classfication_GT;
    Position left;
    Position top;
    Position width;
    Position height;
} ObjectBorder;

static mp_int_t
init_callback(mediapipe_t* mp);

static void*
create_ctx(mediapipe_t* mp);

static void
destroy_ctx(void* _ctx);

static mp_command_t
    mp_teststream_commands[]
    = {
        mp_custom_command0("teststream"),
        mp_null_command
      };

static mp_module_ctx_t
    mp_teststream_module_ctx
    = {
        mp_string("teststream"),
        create_ctx,
        nullptr,
        destroy_ctx
      };

mp_module_t
    mp_teststream_module
    = {
        MP_MODULE_V1,
        &mp_teststream_module_ctx, /* module context */
        mp_teststream_commands, /* module directives */
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
    auto queue = ctx->meta_queue;

    if (g_queue_is_empty(queue)) {
        LOG_WARNING("roi_meta is NULL!");
        return TRUE;
    }

    guint frameId = 0;
    guint top, left, width, height;


    if (g_queue_is_empty(queue)) {
        return TRUE;
    }

    GstStructure* s = (GstStructure*)g_queue_peek_head(queue);
    if (!gst_structure_get_uint(s, "frame_number", &frameId)) {
        LOG_WARNING("unexpected GstStructure object");
        gst_structure_free(s);
        return TRUE;
    }

    while (TRUE) {
        if (!gst_structure_get_uint(s, "left", &left)) {
            LOG_WARNING("unexpected GstStructure object");
            gst_structure_free(s);
            break;
        }
        if (!gst_structure_get_uint(s, "top", &top)) {
            LOG_WARNING("unexpected GstStructure object");
            gst_structure_free(s);
            break;
        }
        if (!gst_structure_get_uint(s, "width", &width)) {
            LOG_WARNING("unexpected GstStructure object");
            gst_structure_free(s);
            break;
        }
        if (!gst_structure_get_uint(s, "height", &height)) {
            LOG_WARNING("unexpected GstStructure object");
            gst_structure_free(s);
            break;
        }

        GstVideoRegionOfInterestMeta* meta = gst_buffer_add_video_region_of_interest_meta(
            buffer, "label", left, top, width, height);
        gst_video_region_of_interest_meta_add_param(meta, s);

        g_queue_pop_head(queue);
        if (g_queue_is_empty(queue)) {
            break;
        }

        guint frameIdNext = 0;
        s = (GstStructure*)g_queue_peek_head(queue);
        if (!gst_structure_get_uint(s, "frame_number", &frameIdNext)) {
            LOG_WARNING("unexpected GstStructure object");
            break;
        }

        if (frameIdNext != frameId) {
            break;
        }
    }

    return TRUE;
}

static void*
create_ctx(mediapipe_t* mp)
{
    g_assert(mp != NULL);
    stream_ctx_t* ctx = g_new0(stream_ctx_t, 1);
    ctx->meta_queue = g_queue_new();
    return ctx;
}

static void
destroy_ctx(void* _ctx)
{
    stream_ctx_t* ctx = (stream_ctx_t*)_ctx;
    if (ctx->meta_queue) {
        g_queue_free_full(ctx->meta_queue, g_free);
    }
    g_free(_ctx);
}

static gboolean
xlinksrc_src_callback(mediapipe_t* mp, GstBuffer* buffer, guint8* data, gsize size, gpointer user_data)
{
    auto ctx = (stream_ctx_t*)user_data;

    GstMapInfo info;
    if (!gst_buffer_map(buffer, &info, GST_MAP_READ)) {
        LOG_ERROR("xlinkreader: gst_buffer_map failed");
        return FALSE;
    }

    auto pData = info.data;
    auto header = reinterpret_cast<Header*>(pData);
    auto metaData = reinterpret_cast<Metadata*>(pData + sizeof(Header));
    auto border = reinterpret_cast<ObjectBorder*>(pData + sizeof(Header) + sizeof(Metadata));

    auto frameId = metaData->frame_number;
    auto headerSize = header->meta_size + sizeof(header);

    for (int i = 0; i < metaData->of_objects; i++) {
        GstVideoRegionOfInterestMeta* meta = gst_buffer_add_video_region_of_interest_meta(
            buffer, "label", border[i].left.u, border[i].top.u, border[i].width.u, border[i].height.u);
        GstStructure* s = gst_structure_new(
            "detection",
            "magic", G_TYPE_UINT, header->magic,
            "version", G_TYPE_UINT, header->version,
            "metaversion", G_TYPE_UINT, metaData->version,
            "stream_id", G_TYPE_UINT, metaData->stream_id,
            "frame_number", G_TYPE_UINT, metaData->frame_number,
            "num_rois", G_TYPE_UINT, metaData->of_objects,
            "classification_index", G_TYPE_UINT, border[i].classfication_GT,
            "reserved", G_TYPE_UINT, border[i].reserved,
            "object_id", G_TYPE_UINT, border[i].object_id,
            "left", G_TYPE_UINT, border[i].left.u,
            "top", G_TYPE_UINT, border[i].top.u,
            "width", G_TYPE_UINT, border[i].width.u,
            "height", G_TYPE_UINT, border[i].height.u, NULL);
        gst_video_region_of_interest_meta_add_param(meta, s);
        g_queue_push_tail(ctx->meta_queue, gst_structure_copy(s));
    }

    gst_buffer_unmap(buffer, &info);

    gst_buffer_resize(buffer, headerSize, -1);

#ifdef DEBUG
    gst_buffer_map(buffer, &info, GST_MAP_READ);
    pData = info.data;
    LOG_INFO("[id:%u] %02X %02X %02X %02X", frameId, pData[64], pData[128], pData[256], pData[512]);
    gst_buffer_unmap(buffer, &info);
#endif

    return TRUE;
}

static mp_int_t init_callback(mediapipe_t* mp)
{
    auto ctx = mp_modules_find_moudle_ctx(mp, "teststream");
    mediapipe_set_user_callback(mp, "src", "src", xlinksrc_src_callback, ctx);
    mediapipe_set_user_callback(mp, "myconvert", "src", myconvert_src_callback, ctx);
    return MP_OK;
}
