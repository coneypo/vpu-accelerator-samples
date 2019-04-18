/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */
#include "mediapipe_com.h"
#include <gst/app/app.h>
#include <vector>

typedef struct {
    GQueue* meta_queue;
    GMutex meta_queue_lock;
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

typedef struct {
    guint8 reserved;
    guint8 object_id;
    guint16 classfication_GT;
    guint16 left;
    guint16 top;
    guint16 width;
    guint16 height;
    guint32 reserved2;
} ObjectBorder;

static mp_int_t
init_callback(mediapipe_t* mp);

static void*
create_ctx(mediapipe_t* mp);

static void
destroy_ctx(void* _ctx);

static char*
config_xlinksrc(mediapipe_t* mp, mp_command_t *cmd);

static mp_command_t mp_teststream_commands[] = {
    {
        mp_string("teststream"),
        MP_MAIN_CONF,
        config_xlinksrc,
        0,
        0,
        NULL
    },
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
    GstStructure* walk, *boxed;
    const GValue *vlist, *item;
    guint nsize;
    guint top, left, width, height;

    g_mutex_lock(&ctx->meta_queue_lock);
    if (g_queue_is_empty(queue)) {
        g_mutex_unlock(&ctx->meta_queue_lock);
        return TRUE;
    }

    walk = (GstStructure*)g_queue_pop_head(queue);
    g_mutex_unlock(&ctx->meta_queue_lock);

    vlist = gst_structure_get_value(walk, "meta_info");
    if (vlist == NULL) {
        LOG_WARNING("unexpected GstStructure object");
        gst_structure_free(walk);
        return TRUE;
    }

    nsize = gst_value_list_get_size(vlist);
    for (guint i = 0; i < nsize; i++) {
        item = gst_value_list_get_value(vlist, i);
        boxed = (GstStructure*)g_value_get_boxed(item);
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
    ctx->meta_queue = g_queue_new();
    g_mutex_init(&ctx->meta_queue_lock);

    return ctx;
}

static void
destroy_ctx(void* _ctx)
{
    stream_ctx_t* ctx = (stream_ctx_t*)_ctx;
    if (ctx->meta_queue) {
        g_queue_free_full(ctx->meta_queue, (GDestroyNotify)gst_structure_free);
    }
    g_mutex_clear(&ctx->meta_queue_lock);
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
    auto metaData = reinterpret_cast<Metadata*>(pData + sizeof(Header));
    auto border = reinterpret_cast<ObjectBorder*>(pData + sizeof(Header) + sizeof(Metadata));

    auto frameId = metaData->frame_number;
    auto headerSize = header->meta_size + sizeof(header) + (metaData->of_objects * sizeof(ObjectBorder));

    for (int i = 0; i < metaData->of_objects; i++) {
        //GstVideoRegionOfInterestMeta* meta = gst_buffer_add_video_region_of_interest_meta(
        //   buffer, "label", border[i].left, border[i].top, border[i].width, border[i].height);
        GValue tmp_value = { 0 };
        g_value_init(&tmp_value, GST_TYPE_STRUCTURE);

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
            "left", G_TYPE_UINT, border[i].left,
            "top", G_TYPE_UINT, border[i].top,
            "width", G_TYPE_UINT, border[i].width,
            "height", G_TYPE_UINT, border[i].height, NULL);

        g_value_take_boxed(&tmp_value, s);
        gst_value_list_append_value(&objectlist, &tmp_value);
        g_value_unset(&tmp_value);
    }

    GstStructure* s = gst_structure_new_empty("detection");
    gst_structure_set_value(s, "meta_info", &objectlist);
    g_value_unset(&objectlist);

    g_mutex_lock(&ctx->meta_queue_lock);
    g_queue_push_tail(ctx->meta_queue, s);
    g_mutex_unlock(&ctx->meta_queue_lock);

    gst_buffer_unmap(buffer, &info);

    gst_buffer_resize(buffer, headerSize, -1);

    return TRUE;
}

static mp_int_t init_callback(mediapipe_t* mp)
{
    auto ctx = mp_modules_find_moudle_ctx(mp, "teststream");
    mediapipe_set_user_callback(mp, "src", "src", xlinksrc_src_callback, ctx);
    mediapipe_set_user_callback(mp, "myconvert", "src", myconvert_src_callback, ctx);
    return MP_OK;
}

static char* config_xlinksrc(mediapipe_t* mp, mp_command_t *cmd)
{
    GstElement* xlinksrc = gst_bin_get_by_name(GST_BIN(mp->pipeline), "src");
    if (xlinksrc) {
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(xlinksrc), "channel")) {
            g_object_set(xlinksrc, "channel", mp->xlink_channel_id, NULL);
        } else {
            printf("Error: cannot find property 'channel' in xlinksrc\n");
        }
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(xlinksrc), "init-xlink")) {
            g_object_set(xlinksrc, "init-xlink", FALSE, NULL);
        } else {
            printf("Error: cannot find property 'init-xlink' in xlinksrc\n");
        }
        gst_object_unref(xlinksrc);
    } else {
        printf("Error: cannot find xlinksrc\n");
    }
    return MP_CONF_OK;
}
