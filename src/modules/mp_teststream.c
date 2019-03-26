/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */
#include "mediapipe_com.h"
#include <gst/app/app.h>

typedef struct {
    GstElement *decode_appsrc;
    GstElement *xlink_appsink;
    GstElement *gva_appsrc;
    GstElement *decode_pipeline;
    GstElement *xlink_pipeline;
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

static GPrivate ctx_key;

static mp_int_t
init_module(mediapipe_t *mp);

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
    NULL,                      /* init_callback */
    NULL,                               /* netcommand_process */
    exit_master,                               /* exit master */
    MP_MODULE_V1_PADDING
};

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis  add meta to buffer and store metadata into queue
 *
 * @Param buf the buffer need to be adden roi meta
 * @Param ctx module context
 */
/* ----------------------------------------------------------------------------*/
static gboolean
create_and_add_meta_to_buffer(GstBuffer *buf, stream_ctx_t *ctx)
{
    g_assert(ctx != NULL);
    g_assert(buf != NULL);
    char *data = ctx->data_head;
    //get the loop number
    int of_objects = ((param_struct_t *)data)->of_objects;
    GstVideoRegionOfInterestMeta *region_meta_p = NULL;
    GstStructure *structure_p = NULL;
    GList *l = NULL;
    GValue value = G_VALUE_INIT;
    //loop add meta
    for (int i = 0; i < of_objects; i++) {
        //malloc the param_struct_t
        param_struct_t *param_data =
            (param_struct_t *)malloc(sizeof(param_struct_t)); //when to release is a problem
        private_metadata_t *roi_metadata = g_new0(private_metadata_t, 1);
        //copy 16 bytes from begin
        memcpy(param_data, data, 16);
        //offset
        data = data + (16 + 16 * i);
        //fill other things
        object_border_t *object_border_p = (object_border_t *)data;
        param_data->reserved = object_border_p->reserved;
        param_data->object_id = object_border_p->object_id;
        param_data->classfication_GT = object_border_p->classfication_GT;
        //add meta to the buffer
        region_meta_p =
            gst_buffer_add_video_region_of_interest_meta(buf, "label_id",
                    object_border_p->left.u, object_border_p->top.u, object_border_p->width.u,
                    object_border_p->height.u);
        structure_p = gst_structure_new_empty("name");
        g_value_init(&value, G_TYPE_POINTER);
        g_value_set_pointer(&value, param_data);
        gst_structure_set_value(structure_p, "structure", &value);
        gst_video_region_of_interest_meta_add_param(region_meta_p, structure_p);
        g_value_unset(&value);
        l = region_meta_p->params;
        g_assert(l != NULL);
        roi_metadata->x =  object_border_p->left.u;
        roi_metadata->y =  object_border_p->top.u;
        roi_metadata->w =  object_border_p->width.u;
        roi_metadata->h =  object_border_p->height.u;
        roi_metadata->param_data = param_data;
        g_queue_push_tail(ctx->meta_queue, roi_metadata);
    }
    return TRUE;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis copy size number bytes from buf to dest
 *
 * @Param dest: destination
 * @Param buf:  buffer
 * @Param size: copy size
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static gboolean
copy(char *dest, GstBuffer *buf, int size)
{
    g_assert(dest != NULL);
    g_assert(buf != NULL);
    g_assert(size >= 0);
    GstMapInfo info;
    GstMapFlags mapFlag = GST_MAP_READ;
    guint8 *pic = NULL;
    if (!gst_buffer_map(buf, &info, mapFlag)) {
        LOG_ERROR(" map buffer error!");
        return FALSE;
    }
    pic = info.data;
    memcpy(dest, pic, size);
    gst_buffer_unmap(buf, &info);
    return TRUE;
}

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
    GstStructure *structure = NULL;
    GList *l = NULL;
    stream_ctx_t *ctx = (stream_ctx_t *) user_data;
    GValue value = G_VALUE_INIT;
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    GstVideoRegionOfInterestMeta *region_meta_p = NULL;
    GstStructure *structure_p = NULL;

    //get the loop number
    private_metadata_t *roi_meta = (private_metadata_t *) g_queue_peek_head(
                                       ctx->meta_queue);
    if (roi_meta == NULL) {
        LOG_WARNING("roi_meta is NULL!");
        return  GST_PAD_PROBE_OK;
    }
    param_struct_t *params = roi_meta->param_data;
    int of_objects = params->of_objects;

    //loop add meta
    for (int i = 0; i < of_objects; i++) {
        roi_meta = (private_metadata_t *) g_queue_pop_head(
                ctx->meta_queue);
        if (roi_meta == NULL) {
            LOG_WARNING("roi_meta is NULL!");
            return  GST_PAD_PROBE_OK;
        }

        //add meta to the buffer
        region_meta_p =
            gst_buffer_add_video_region_of_interest_meta(buffer, "label_id", roi_meta->x,
                    roi_meta->y, roi_meta->w, roi_meta->h);
        LOG_DEBUG("decode callback:x:%u,y:%u, w:%u, w:%u", roi_meta->x, roi_meta->y,
                roi_meta->w, roi_meta->h);
        structure_p = gst_structure_new_empty("name");
        g_value_init(&value, G_TYPE_POINTER);
        g_value_set_pointer(&value, roi_meta->param_data);
        gst_structure_set_value(structure_p, "structure", &value);
        gst_video_region_of_interest_meta_add_param(region_meta_p, structure_p);
        g_value_unset(&value);
        g_free(roi_meta);
    }

    return GST_PAD_PROBE_OK;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis videoconvert src callback to pushbufer to mp->pipeline
 *
 * @Param pad
 * @Param info
 * @Param user_data
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static GstPadProbeReturn Videoconvert_src_callback(GstPad *pad,
        GstPadProbeInfo *info,
        gpointer user_data)
{
    GstCaps *caps = NULL;
    GstElement *appsrc = NULL;
    GstBuffer *buffer = NULL;
    g_assert(user_data != NULL);
    caps = gst_pad_get_current_caps(pad);
    appsrc = (GstElement *)user_data;
    g_object_set(appsrc, "caps", caps, NULL);
    gst_caps_unref(caps);
    buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    g_signal_emit_by_name(appsrc, "push-buffer", buffer, NULL);
    return GST_PAD_PROBE_OK;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis  recursion progress all data in the buffer, get meta data, spilt or merge
 *            data into a whole frame data. and then push data to decode pipeline
 *
 * @Param buf1       the buffet need to be progress
 * @Param spilt_buf_num   whole frame be pushed
 * @Param ctx   module context
 *
 * @Returns  if there is enough data for head and meta return true, else return false
 */
/* ----------------------------------------------------------------------------*/
static gboolean
progress(GstBuffer *buf1, int *spilt_buf_num, stream_ctx_t *ctx)
{
    g_assert(buf1 != NULL);
    g_assert(spilt_buf_num != NULL);
    g_assert(ctx != NULL);
    char *data = ctx->data_head;
    int *pkg_offset = &ctx->pkg_offset;
    GstElement *appsrc = ctx->decode_appsrc;
    guint current_buffer_size = gst_buffer_get_size(buf1);
    LOG_DEBUG("current_buffer_size:%d\n", current_buffer_size);
    LOG_DEBUG("pkg_offset:%d\n", *pkg_offset);
    GstBuffer *buf = gst_buffer_copy(buf1);
    int meta_size = 0;
    int total_size = 0;
    int HEADSIZE = 8;
    GstFlowReturn ret;
    int stream_package_size = 0;
    int old_pkg_offset = *pkg_offset;
    GstBuffer *buf_new1 = NULL;
    GstBuffer *merge_buffer = NULL;
    //the package header is not enough
    if (old_pkg_offset + current_buffer_size < HEADSIZE) {
        copy(data + *pkg_offset, buf, current_buffer_size);
        *pkg_offset += current_buffer_size;
        return FALSE;
    } else { //header is enough
        if (HEADSIZE > old_pkg_offset) {
            copy(data + old_pkg_offset, buf, HEADSIZE - old_pkg_offset);
            *pkg_offset = HEADSIZE;
        }
        meta_size = *((short *)&data[2]);
        total_size = *((int *)&data[4]);
        LOG_DEBUG("meta_size:%d, total_size:%d\n", meta_size, total_size);
        //meta is not enough
        if (old_pkg_offset + current_buffer_size < meta_size + HEADSIZE) {
            copy(data + old_pkg_offset, buf, current_buffer_size);
            *pkg_offset = old_pkg_offset + current_buffer_size;
            return FALSE;
        } else { //meta is enough
            if (HEADSIZE + meta_size > old_pkg_offset) {
                copy(data + old_pkg_offset, buf, HEADSIZE + meta_size - old_pkg_offset);
                *pkg_offset = HEADSIZE + meta_size;
            }
            stream_package_size = current_buffer_size - *pkg_offset + old_pkg_offset;
            if (current_buffer_size + old_pkg_offset >
                total_size) { //buffer data over a whole package
                if (*pkg_offset == old_pkg_offset) {
                    stream_package_size = total_size - *pkg_offset;
                } else {
                    stream_package_size = total_size - *pkg_offset + old_pkg_offset;
                }
                //get whole and half data at the buffer beginning
                gst_buffer_resize(buf,  *pkg_offset - old_pkg_offset, stream_package_size);
                GST_BUFFER_OFFSET(buf) = ctx->total_offset;
                GST_BUFFER_OFFSET_END(buf) = ctx->total_offset + stream_package_size;
                ctx->total_offset += stream_package_size;
                buf_new1 = gst_buffer_copy(buf);
                if (ctx->half_buffer != NULL) {
                    //merge last half buffer and current halt buffer
                    merge_buffer = gst_buffer_append(ctx->half_buffer, buf);
                    GST_BUFFER_OFFSET(merge_buffer) = GST_BUFFER_OFFSET(ctx->half_buffer);
                    GST_BUFFER_OFFSET_END(merge_buffer) =
                        GST_BUFFER_OFFSET_END(ctx->half_buffer) + stream_package_size;
                    ctx->half_buffer = NULL;
                } else {
                    //it's a whole buffer
                    merge_buffer = buf;
                }
                LOG_DEBUG("merge_buffer:%ld\n", gst_buffer_get_size(merge_buffer));
                //add meta
                create_and_add_meta_to_buffer(merge_buffer, ctx);
                //send buffer
                g_signal_emit_by_name(appsrc, "push-buffer", merge_buffer, &ret);
                if (ret != GST_FLOW_OK) {
                    LOG_ERROR(" push buffer error\n");
                }
                gst_buffer_unref(merge_buffer);
                *spilt_buf_num += 1;
                //progress left data
                gst_buffer_resize(buf_new1,  stream_package_size,
                                  current_buffer_size - stream_package_size - *pkg_offset + old_pkg_offset);
                *pkg_offset = 0;
                progress(buf_new1, spilt_buf_num, ctx);
                gst_buffer_unref(buf_new1);
                return TRUE;
            } else if (current_buffer_size + *pkg_offset ==
                       total_size) {// buffet data  =  whole package
                gst_buffer_resize(buf, *pkg_offset - old_pkg_offset, stream_package_size);
                GST_BUFFER_OFFSET(buf) = ctx->total_offset;
                GST_BUFFER_OFFSET_END(buf) = ctx->total_offset + stream_package_size;
                ctx->total_offset += stream_package_size;
                create_and_add_meta_to_buffer(buf, ctx);
                g_signal_emit_by_name(appsrc, "push-buffer", buf, &ret);
                gst_buffer_unref(buf);
                if (ret != GST_FLOW_OK) {
                    LOG_ERROR(" push buffer error\n");
                }
                *pkg_offset = 0;
                *spilt_buf_num += 1;
                return TRUE;
            } else {// buffet data  <  whole package
                gst_buffer_resize(buf, *pkg_offset - old_pkg_offset, stream_package_size);
                GST_BUFFER_OFFSET(buf) = ctx->total_offset;
                GST_BUFFER_OFFSET_END(buf) = ctx->total_offset + stream_package_size;
                ctx->total_offset += stream_package_size;
                if (ctx->half_buffer != NULL) {
                    merge_buffer = gst_buffer_append(ctx->half_buffer, buf);
                    GST_BUFFER_OFFSET(merge_buffer) = GST_BUFFER_OFFSET(ctx->half_buffer);
                    GST_BUFFER_OFFSET_END(merge_buffer) = GST_BUFFER_OFFSET_END(
                            ctx->half_buffer) + stream_package_size;
                } else {
                    merge_buffer = buf;
                }
                ctx->half_buffer = merge_buffer;
                *pkg_offset += stream_package_size;
                return TRUE;
            }
        }
    }
    return FALSE;
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
static void
decode_need_data(GstElement *appsrc,
                 guint       unused_size,
                 gpointer    user_data)
{
    g_assert(user_data != NULL);
    static guint64 offset = 0;
    stream_ctx_t *ctx = (stream_ctx_t *)user_data;
    GstElement *appsink = (GstElement *) ctx->xlink_appsink;
    GstSample *sample;
    guint split_buffer_num = 0;
    GstBuffer *buffer = NULL;
    g_signal_emit_by_name(ctx->xlink_appsink, "pull-sample", &sample);
    if (sample) {
        buffer = gst_sample_get_buffer(sample);
        progress(buffer, &split_buffer_num, ctx);
        if (split_buffer_num == 0) {
            decode_need_data(appsrc, unused_size, user_data);
        }
        gst_sample_unref(sample);
    } else {
        decode_need_data(appsrc, unused_size, user_data);
    }
}

static mp_int_t
init_module(mediapipe_t *mp)
{
    g_assert(mp != NULL);
    GstElement *h264decode = NULL;
    GstElement *Videoconvert = NULL;
    GstPad *h264srcpad = NULL;
    GstPad *convertpad = NULL;
    GstStateChangeReturn ret =  GST_STATE_CHANGE_SUCCESS;
    stream_ctx_t *ctx = (stream_ctx_t *)mp_modules_find_moudle_ctx(mp,
                        "teststream");
    //create branch pipeline
    ctx->decode_pipeline =
        mediapipe_branch_create_pipeline("appsrc name=mysrc ! video/x-h264,\
                width=1920,height=1088,stream-format=byte-stream,alignment=au, \
                profile=(string)constrained-baseline, framerate=(fraction)0/1 ! \
                vaapih264dec name=my264dec ! videoconvert name=myconvert ! \
                video/x-raw, format=BGRA ! fakesink");
    ctx->xlink_pipeline =
        mediapipe_branch_create_pipeline("xlinksrc ! appsink name=sink");
    if (ctx->decode_pipeline == NULL || ctx->xlink_pipeline == NULL) {
        LOG_ERROR("teststream: create pipeline failed !");
        return  MP_ERROR;
    }
    //get the useful element  from pipeline
    ctx->decode_appsrc = gst_bin_get_by_name(GST_BIN(ctx->decode_pipeline),
                         "mysrc");
    ctx->xlink_appsink = gst_bin_get_by_name(GST_BIN(ctx->xlink_pipeline), "sink");
    ctx->gva_appsrc = gst_bin_get_by_name(GST_BIN(mp->pipeline), "src12");
    //record and set
    g_object_set(G_OBJECT(ctx->decode_appsrc),
                 "stream-type", 0,
                 "format", GST_FORMAT_TIME, NULL);
    g_signal_connect(ctx->decode_appsrc, "need-data", G_CALLBACK(decode_need_data),
                     ctx);
    g_object_set(G_OBJECT(ctx->gva_appsrc),
                 "stream-type", 0,
                 "format", GST_FORMAT_TIME, NULL);
    //Add callback to vaapih264dec
    h264decode = gst_bin_get_by_name(GST_BIN(ctx->decode_pipeline), "my264dec");
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
    //Add callback to videoconvert
    Videoconvert = gst_bin_get_by_name(GST_BIN(ctx->decode_pipeline),
                                       "myconvert");
    if (Videoconvert == NULL) {
        LOG_ERROR("can't find element myconvert");
        return MP_ERROR;
    }
    convertpad = gst_element_get_static_pad(Videoconvert, "src");
    if (convertpad == NULL) {
        LOG_ERROR("can't find pad convert src pad");
        return MP_ERROR;
    }
    gst_pad_add_probe(convertpad, GST_PAD_PROBE_TYPE_BUFFER,
                      Videoconvert_src_callback, ctx->gva_appsrc,
                      NULL);
    gst_object_unref(convertpad);
    gst_object_unref(Videoconvert);
    //run pipeline
    ret = gst_element_set_state(ctx->decode_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return MP_ERROR;
    }
    //run pipeline1
    ret = gst_element_set_state(ctx->xlink_pipeline, GST_STATE_PLAYING);
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
    if (ctx->decode_appsrc) {
        gst_object_unref(ctx->decode_appsrc);
    };
    if (ctx->gva_appsrc) {
        gst_object_unref(ctx->gva_appsrc);
    };
    if (ctx->xlink_appsink) {
        gst_object_unref(ctx->xlink_pipeline);
    };
    if (ctx->decode_pipeline) {
        gst_object_unref(ctx->decode_pipeline);
    };
    if (ctx->xlink_pipeline) {
        gst_object_unref(ctx->xlink_pipeline);
    };
    if (ctx->half_buffer) {
        gst_buffer_unref(ctx->half_buffer);
    }
    if (ctx->meta_queue) {
        g_queue_free_full(ctx->meta_queue, g_free);
    }
    g_free(_ctx);
}
