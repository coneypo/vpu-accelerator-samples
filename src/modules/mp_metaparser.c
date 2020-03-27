/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */
#include "mediapipe.h"
#include "mediapipe_com.h"
#include "utils/packet_struct_v3.h"

#include <gst/app/app.h>
#include <vector>
#ifdef __cplusplus
extern "C" {
#endif
#include <hddl/gsthddlcontext.h>
#ifdef __cplusplus
}
#endif

#define XLINK_DEVICE_PATH	"/tmp/xlink_mock"
#define XLINK_DEVICE_TYPE	PCIE_DEVICE
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

#if SWITCHON
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
#endif

static gboolean
insert_metainfo_src_callback(mediapipe_t* mp, GstBuffer* buffer, guint8* data, gsize size, gpointer user_data)
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

    guint nsize = gst_value_list_get_size(vlist);

    GstMeta *gst_meta = NULL;
    gpointer state = NULL;

    if(nsize > 0)
    {
        const GValue* item = gst_value_list_get_value(vlist, 0);
        GstStructure* boxed = (GstStructure*)g_value_get_boxed(item);
        guint meta_num = 0 ;
        while ((gst_meta = gst_buffer_iterate_meta(buffer, &state)) != NULL) {
            if (gst_meta->info->api != GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE) {
                continue ;
            }
            meta_num++;
            gst_video_region_of_interest_meta_add_param((GstVideoRegionOfInterestMeta*)gst_meta, gst_structure_copy(boxed));
        }
        //add a fake roi into buffer to store stream info,the info will be push back to host
        if(meta_num == 0){
            GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(
                    buffer, "mediapipe_fake_roi", 0, 0, 0, 0);
            gst_video_region_of_interest_meta_add_param(meta, gst_structure_copy(boxed));
        }
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
src_src_callback(mediapipe_t* mp, GstBuffer* buffer, guint8* data, gsize size, gpointer user_data)
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
    UNUSED(frameId);
    auto headerSize = header->meta_size + sizeof(header) + (meta->num_rois * sizeof(ROI));

    for (int i = 0; i < meta->num_rois; i++) {
        //GstVideoRegionOfInterestMeta* meta = gst_buffer_add_video_region_of_interest_meta(
        //   buffer, "label", border[i].left, border[i].top, border[i].width, border[i].height);
        GValue tmp_value = { 0 };
        g_value_init(&tmp_value, GST_TYPE_STRUCTURE);

        GstStructure* s = gst_structure_new(
            "mediapipe_probe_meta",
            "magic", G_TYPE_UINT, header->magic,
            "version", G_TYPE_UINT, header->version,
            "metaversion", G_TYPE_UINT, meta->version,
            "packet_type", G_TYPE_UINT, meta->packet_type,
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

    //even if there is no roi,  I also need to store the stream info
    if(meta->num_rois == 0){
        GValue tmp_value0 = { 0 };
        g_value_init(&tmp_value0, GST_TYPE_STRUCTURE);
        GstStructure* s0 = gst_structure_new(
                "mediapipe_probe_meta",
                "magic", G_TYPE_UINT, header->magic,
                "version", G_TYPE_UINT, header->version,
                "metaversion", G_TYPE_UINT, meta->version,
                "packet_type", G_TYPE_UINT, meta->packet_type,
                "stream_id", G_TYPE_UINT, meta->stream_id,
                "frame_number", G_TYPE_UINT, meta->frame_number,
                "num_rois", G_TYPE_UINT, meta->num_rois,
                "classification_index", G_TYPE_UINT, 0,
                "reserved", G_TYPE_UINT, 0,
                "object_index", G_TYPE_UINT, 0,
                "left", G_TYPE_UINT, 0,
                "top", G_TYPE_UINT, 0,
                "width", G_TYPE_UINT, 0,
                "height", G_TYPE_UINT, 0, NULL);

        g_value_take_boxed(&tmp_value0, s0);
        gst_value_list_append_value(&objectlist, &tmp_value0);
        g_value_unset(&tmp_value0);
    }

    GstStructure* s = gst_structure_new_empty("mediapipe_probe_meta");
    gst_structure_set_value(s, "meta_info", &objectlist);
    g_value_unset(&objectlist);

    g_async_queue_push(ctx->meta_queue, s);

    gst_buffer_unmap(buffer, &info);

    gst_buffer_resize(buffer, headerSize, -1);

    return TRUE;
}

static mp_int_t init_callback(mediapipe_t* mp)
{
    auto ctx = mp_modules_find_module_ctx(mp, "metaparser");
    mediapipe_set_user_callback(mp, "src", "src", src_src_callback, ctx);
    /* mediapipe_set_user_callback(mp, "myconvert", "src", myconvert_src_callback, ctx); */
    mediapipe_set_user_callback(mp, "detect", "src", insert_metainfo_src_callback, ctx);
    return MP_OK;
}

static char* load_config(mediapipe_t* mp, mp_command_t* cmd)
{
    int channelId = -1;
    if (!mediapipe_get_channelId(mp, "src", &channelId)) {
        return MP_CONF_OK;
    }

    GstElement* src = gst_bin_get_by_name(GST_BIN(mp->pipeline), "src");
    if (!src) {
        LOG_WARNING("cannot find element named \"src\".");
        return MP_CONF_OK;
    }

    if (g_object_class_find_property(G_OBJECT_GET_CLASS(src), "selected-target-context")) {
        GstHddlContext *context = gst_hddl_context_new (CONNECT_XLINK);
        context->hddl_xlink->xlink_handler->dev_type = XLINK_DEVICE_TYPE;
        uint32_t sw_device_id_list[10];
        uint32_t num_devices;
        //TODO: fix hard-coded pid 0x6240
        int ret = xlink_get_device_list(sw_device_id_list, &num_devices, 0x6240);
        assert(ret == 0);
	    ret++;
        context->hddl_xlink->xlink_handler->sw_device_id = sw_device_id_list[0];
        context->hddl_xlink->channelId = channelId;
        //use xlink_connect to get correct link_id
        xlink_connect(context->hddl_xlink->xlink_handler);
        ret = xlink_close_channel(context->hddl_xlink->xlink_handler, channelId);
        g_object_set(src, "selected-target-context", context, NULL);
        gst_hddl_context_free(context);
        LOG_INFO("set \"selected-target-context\" with channelId(%d) on element named \"src\".", channelId);
    } else {
        LOG_WARNING("cannot find property \"selected-target-context\" on element named \"src\".");
    }

    gst_object_unref(src);

    return MP_CONF_OK;
}
