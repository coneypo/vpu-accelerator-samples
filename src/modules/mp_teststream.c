/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */
#include "mediapipe_com.h"
#include <gst/app/app.h>
#include <vector>

typedef struct {
    GstElement *decode_pipeline;
    char data_head[1024];
    int pkg_offset;
    GstBuffer *half_buffer;
    guint64 total_offset;
    GQueue *meta_queue;
} stream_ctx_t;

/* This structure is GstVideoRegionOfInterestMeta's params */
typedef struct {
    /*header*/
    guint8  magic;
    guint8  header_version;
    guint16 meta_size;
    guint32 package_size;

    /*Metadata*/
    guint8  meta_version;
    guint8  packet_type;
    guint8  stream_id;
    guint8  of_objects;
    guint32 frame_number;

    /*object*/
    guint8  reserved;
    guint8  object_id;
    guint16 classfication_GT;
} param_struct_t;

#pragma pack(push)
#pragma pack(1)
typedef struct {
    unsigned u : 24;
} foo_t;
#pragma pack(pop)

typedef struct {
    /*object*/
    guint8  reserved;
    guint8  object_id;
    guint16 classfication_GT;
    foo_t left;
    foo_t top;
    foo_t width;
    foo_t height;
} object_border_t;

typedef struct {
    /*object*/
    guint x;
    guint y;
    guint w;
    guint h;
    param_struct_t  *param_data;
} private_metadata_t;

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

static GPrivate ctx_key;

static mp_int_t
init_module(mediapipe_t *mp);

static mp_int_t init_callback(mediapipe_t* mp);

static void
exit_master();

static void *
create_ctx(mediapipe_t *mp);

static void
destroy_ctx(void *_ctx);

static gboolean
create_and_add_meta_to_buffer(GstBuffer *buf, stream_ctx_t *ctx);

static gboolean
copy(char *dest, GstBuffer *buf, int size);

static gboolean
progress(GstBuffer *buf1, int *spilt_buf_num, stream_ctx_t *ctx);

static void
decode_need_data(GstElement *appsrc, guint unused_size, gpointer user_data);

static GstPadProbeReturn
vaapih264decode_src_callback(GstPad *pad, GstPadProbeInfo *info,
                             gpointer user_data);

static mp_command_t
mp_teststream_commands[] = {
    {
        mp_string("teststream"),
        MP_MAIN_CONF,
        NULL,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_module_ctx_t
mp_teststream_module_ctx = {
    mp_string("teststream"),
    create_ctx,
    NULL,
    destroy_ctx
};

mp_module_t
mp_teststream_module = {
    MP_MODULE_V1,
    &mp_teststream_module_ctx,                /* module context */
    mp_teststream_commands,                   /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                               /* init master */
    init_module,                               /* init module */
    NULL,                               /* keyshot_process*/
    NULL,                               /* message_process */
    init_callback,                      /* init_callback */
    NULL,                               /* netcommand_process */
    exit_master,                               /* exit master */
    MP_MODULE_V1_PADDING
};

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis vaapih264dec src callback to transfer meta
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static GstPadProbeReturn
vaapih264decode_src_callback(GstPad *pad, GstPadProbeInfo *info,
                             gpointer user_data)
{
    auto ctx = (stream_ctx_t*)user_data;
    auto queue = ctx->meta_queue;

    if (g_queue_is_empty(queue)) {
        LOG_WARNING("roi_meta is NULL!");
        return GST_PAD_PROBE_OK;
    }

    guint frameId = 0;
    guint top, left, width, height;

    GstStructure* s = (GstStructure*)g_queue_pop_head(queue);
    if (!gst_structure_get_uint(s, "frameId", &frameId)) {
        LOG_WARNING("unexpected GstStructure object");
        gst_structure_free(s);
        return GST_PAD_PROBE_OK;
    }

    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);

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

        GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(
            buffer, "label", left, top, width, height);
        gst_video_region_of_interest_meta_add_param(meta, s);

        if (g_queue_is_empty(queue)) {
            break;
        }

        guint frameIdNext = 0;
        s = (GstStructure*)g_queue_peek_head(queue);
        if (!gst_structure_get_uint(s, "frameId", &frameIdNext)) {
            LOG_WARNING("unexpected GstStructure object");
            break;
        }

        if (frameIdNext != frameId) {
            break;
        }
    }

    return GST_PAD_PROBE_OK;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis  decode pipeline appsrc need-data signal responding func
 *
 * @Param appsrc
 * @Param unused_size
 * @Param user_data
 */
/* ----------------------------------------------------------------------------*/

static mp_int_t
init_module(mediapipe_t *mp)
{
    g_assert(mp != NULL);
    GstElement *h264decode = NULL;
    GstElement *Videoconvert = NULL;
    GstPad *h264srcpad = NULL;
    GstPad *convertpad = NULL;
    GstStateChangeReturn ret =  GST_STATE_CHANGE_SUCCESS;
    stream_ctx_t *ctx = (stream_ctx_t *)mp_modules_find_moudle_ctx(mp, "teststream");
    ctx->decode_pipeline = mp->pipeline;
    if (ctx->decode_pipeline == NULL) {
        LOG_ERROR("teststream: create pipeline failed !");
        return  MP_ERROR;
    }

    //Add callback to vaapih264dec
    h264decode = gst_bin_get_by_name(GST_BIN(ctx->decode_pipeline), "myconvert");
    if (h264decode == NULL) {
        LOG_ERROR("can't find element parse");
        return MP_IGNORE;
    }
    h264srcpad = gst_element_get_static_pad(h264decode, "src");
    if (h264srcpad) {
        gst_pad_add_probe(h264srcpad, GST_PAD_PROBE_TYPE_BUFFER,
                          vaapih264decode_src_callback, ctx,
                          NULL);
        gst_object_unref(h264srcpad);
    }
    gst_object_unref(h264decode);

    //run pipeline
    ret = gst_element_set_state(ctx->decode_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return MP_ERROR;
    }
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return MP_ERROR;
    }
    //new a queue
    if (NULL == ctx->meta_queue) {
        ctx->meta_queue = g_queue_new();
    }
    return MP_OK;
}

static void
exit_master()
{
}

static void *
create_ctx(mediapipe_t *mp)
{
    g_assert(mp != NULL);
    stream_ctx_t *ctx = g_new0(stream_ctx_t, 1);
    return ctx;
}

static void
destroy_ctx(void *_ctx)
{
    stream_ctx_t *ctx = (stream_ctx_t *) _ctx;
    if (ctx->half_buffer) {
        gst_buffer_unref(ctx->half_buffer);
    }
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
        GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(
            buffer, "label", border[i].left.u, border[i].top.u, border[i].width.u, border[i].height.u);
        GstStructure *s = gst_structure_new(
            "detection", "reserved", G_TYPE_UINT, border[i].reserved, "object_id", G_TYPE_UINT, border[i].object_id,
            "classfication_GT", G_TYPE_UINT, border[i].classfication_GT, "frameId", G_TYPE_UINT, frameId, "left", G_TYPE_UINT, border[i].left.u,
            "top", G_TYPE_UINT, border[i].top.u, "width", G_TYPE_UINT, border[i].width.u, "height", G_TYPE_UINT, border[i].height.u, NULL);
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

#define GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buf, state)                                                          \
    ((GstVideoRegionOfInterestMeta *)gst_buffer_iterate_meta_filtered(buf, state,                                      \
                                                                      GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE))

static gboolean
gvaclassify_sink_callback(mediapipe_t* mp, GstBuffer* buffer, guint8* data, gsize size, gpointer user_data)
{
    std::vector<GstVideoRegionOfInterestMeta *> metas;
    GstVideoRegionOfInterestMeta *meta = NULL;
    gpointer state = NULL;
    while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
        metas.push_back(meta);
    }

    LOG_INFO("size=%lu", metas.size());

    return TRUE;
}

static mp_int_t init_callback(mediapipe_t* mp)
{
    auto ctx = mp_modules_find_moudle_ctx(mp, "teststream");
    mediapipe_set_user_callback(mp, "src", "src", xlinksrc_src_callback, ctx);
    mediapipe_set_user_callback(mp, "classify", "sink", gvaclassify_sink_callback, ctx);
    return MP_OK;
}
