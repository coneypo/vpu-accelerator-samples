/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mediapipe_com.h"
#include <string>

#define MESSAGE_MIX3_MAX_NUM 50

/*This struct is used to store
 * info of A type message.
 */
typedef struct {
    const char *elem_name;
    const char *message_name;
    void *pro_fun;
    GQueue *message_queue;
    GMutex lock;
    guint delay;
    const char *delay_queue_name;
    guint draw_color_channel[3];
} mix3_message_ctx;

/* This struct is used to store
 * All message info、message
 * process func and types of message
 */
typedef struct {
    GHashTable *msg_pro_fun_hst;
    mix3_message_ctx msg_ctxs[MESSAGE_MIX3_MAX_NUM];
    guint msg_ctx_num;
} mix3_ctx;

static gboolean
mix3_src_callback(mediapipe_t *mp, GstBuffer *buf, guint8 *data, gsize size,
                  gpointer user_data);
#if SWITCHON
static gboolean
mix3_draw_text(GstBuffer *buffer, GstVideoInfo *info, int x, int y,
               int width, int height, const gchar *text);
#endif

static char *
mp_mix3_block(mediapipe_t *mp, mp_command_t *cmd);

static gboolean
json_analyse_and_post_message(mediapipe_t *mp, const gchar *elem_name);

static mp_int_t
process_tracking_message(const char *message_name,
                                  const char *subscribe_name,
                                  mediapipe_t *mp, GstMessage *message);

static mp_int_t
queue_message_from_observer(const char *message_name,
                            const char *subscribe_name,
                            mediapipe_t *mp, GstMessage *message);
static gboolean
draw_buffer_by_message(mediapipe_t *mp, mix3_message_ctx *msg_ctx,
                       GstBuffer *buffer, const char *element_name);
static gboolean
change_metainfo_by_width_height(GstBuffer *currentBuffer, guint currentBufferWidth,
                             guint currentBufferHeight, guint branchBufferWidth,
                             guint branchBufferHeight);

static void *create_ctx(mediapipe_t *mp);
static void destroy_ctx(void *ctx);

static mp_command_t  mp_mix3_commands[] = {
    {
        mp_string("mix3"),
        MP_MAIN_CONF,
        mp_mix3_block,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_module_ctx_t  mp_mix3_module_ctx = {
    mp_string("mix3"),
    create_ctx,
    NULL,
    destroy_ctx
};

mp_module_t  mp_mix3_module = {
    MP_MODULE_V1,
    &mp_mix3_module_ctx,                /* module context */
    mp_mix3_commands,                   /* module directives */
    MP_CORE_MODULE,                     /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    NULL,                               /* keyshot_process*/
    NULL,                               /* message_process */
    NULL,                               /* init_callback */
    NULL,                               /* netcommand_process */
    NULL,                               /* exit master */
    MP_MODULE_V1_PADDING
};

static mp_int_t
queue_message_from_observer(const char *message_name,
                            const char *subscribe_name,
                            mediapipe_t *mp, GstMessage *message)
{
    mix3_ctx *ctx = (mix3_ctx *) mp_modules_find_module_ctx(mp, "mix3");
    for (guint i = 0; i < ctx->msg_ctx_num; i++) {
        if (0 == g_strcmp0(message_name, ctx->msg_ctxs[i].message_name)
            && 0 == g_strcmp0(subscribe_name, ctx->msg_ctxs[i].elem_name)) {
            g_mutex_lock(&ctx->msg_ctxs[i].lock);
            g_queue_push_tail(ctx->msg_ctxs[i].message_queue,
                              gst_message_ref(message));
            g_mutex_unlock(&ctx->msg_ctxs[i].lock);
        }
    }
    return MP_OK;
}

static mp_int_t
process_tracking_message(const char *message_name,
                                  const char *subscribe_name,
                                  mediapipe_t *mp, GstMessage *message)
{
    return queue_message_from_observer(message_name, subscribe_name,
                                       mp, message);
}

static char *
mp_mix3_block(mediapipe_t *mp, mp_command_t *cmd)
{
    if (mp->config == NULL) {
        return (char *) MP_CONF_ERROR;
    };
    json_object_object_foreach(mp->config, key, val) {
	UNUSED(val);
        if (NULL != strstr(key, "mix3")) {
            json_analyse_and_post_message(mp, key);
        }
    }
    return MP_CONF_OK;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis draw border on frame by param
 *
 * @Param buffer frame buffer
 * @Param pic_w frame width
 * @Param pic_h frame height
 * @Param rect_x x of border's left top point
 * @Param rect_y y of border's left top point
 * @Param rect_w width of border
 * @Param rect_h height of border
 * @Param R RGB color's Red value
 * @Param G RGB color's Green value
 * @Param B RGB color's Blue value
 *
 * @Returns 0 is success, -1 is map error.
 */
/* ----------------------------------------------------------------------------*/
#if SWITCHON
static gint
nv12_border(GstBuffer *buffer, guint pic_w, guint pic_h, guint rect_x,
            guint rect_y, guint rect_w, guint rect_h, int R, int G, int B)
{
    GstMapInfo info;
    GstMapFlags mapFlag = GstMapFlags(GST_MAP_READ | GST_MAP_WRITE);
    //map buf to info
    //
    if (! gst_buffer_map(buffer, &info, mapFlag)) {
        LOG_ERROR("mix3: map buffer error!");
        return -1;
    }
    guint8 *pic = info.data;
    /* judge the params*/
    if (rect_y > pic_h || rect_x > pic_w) {
        gst_buffer_unmap(buffer, &info);
        return 0;
    }
    if (rect_y + rect_h > pic_h) {
        rect_h = pic_h - rect_y;
    }
    if (rect_x + rect_w > pic_w) {
        rect_w = pic_w - rect_x;
    }
    /* set up the rectangle border size */
    const int border = 5;
    /* RGB convert YUV */
    int Y = 0, U, V;
    Y = (0.257 * R) + (0.504 * G) + (0.098 * B) + 16;
    V = (0.439 * R) - (0.368 * G) - (0.071 * B) + 128;
    U = -(0.148 * R) - (0.291 * G) + (0.439 * B) + 128;

    /* Locking the scope of rectangle border range */
    guint j, k;
    for (j = rect_y; j < rect_y + rect_h; j++) {
        for (k = rect_x; k < rect_x + rect_w; k++) {
            if (k < (rect_x + border) || k > (rect_x + rect_w - border) ||
                j < (rect_y + border) || j > (rect_y + rect_h - border)) {
                /* Components of YUV's storage address index */
                int y_index = j * pic_w + k;
                int u_index = (y_index / 2 - pic_w / 2 * ((j + 1) / 2)) * 2 + pic_w * pic_h;
                int v_index = u_index + 1;
                /* set up YUV's conponents value of rectangle border */
                pic[y_index] =  Y ;
                pic[u_index] =  U ;
                pic[v_index] =  V ;
            }
        }
    }
    gst_buffer_unmap(buffer, &info);
    return 0;
}
#endif

static gboolean
draw_buffer_by_message(mediapipe_t *mp, mix3_message_ctx *msg_ctx,
                       GstBuffer *buffer, const char *element_name)
{
    GstMessage *walk;
    guint mp_ret = 0;
    UNUSED(mp_ret);
    GstClockTime msg_pts;
    GstClockTime offset = 300000000;
    GstClockTime desired = GST_BUFFER_PTS(buffer);
    const GValue *brunchBufferValue;
    const GstStructure *root;
    GstBuffer *branchBuffer = NULL;
    GstMeta *gst_meta = NULL;
    UNUSED(gst_meta);
    gpointer state = NULL;
    UNUSED(state);
    guint x, y, width, height;
    UNUSED(x);
    UNUSED(y);
    UNUSED(width);
    UNUSED(height);
    GstElement *enc_element = NULL;
    GstPad *src_pad = NULL;
    GstCaps *src_caps = NULL;
    //get element
    enc_element = gst_bin_get_by_name(GST_BIN((mp)->pipeline), element_name);
    if (enc_element != NULL) {
        src_pad = gst_element_get_static_pad(enc_element, "src");
        if (src_pad == NULL) {
            src_pad = gst_element_get_request_pad(enc_element, "src%d");
        }
        if (src_pad != NULL) {
            src_caps = gst_pad_get_current_caps(src_pad);
            gst_object_unref(src_pad);
        }
        gst_object_unref(enc_element);
    } else {
        LOG_WARNING("mix3: Have no element named \"%s\"! ", element_name);
    }
    //get width height
    guint _width;
    guint _height;
    guint branchBufferWidth;
    guint branchBufferHeight;
    gboolean ret2 = FALSE;
    GstVideoInfo src_video_info;
    if (src_caps != NULL) {
        ret2 = gst_video_info_from_caps(&src_video_info, src_caps);
        if (ret2) {
            _width = GST_VIDEO_INFO_WIDTH(&src_video_info);
            _height = GST_VIDEO_INFO_HEIGHT(&src_video_info);
        } else {
            LOG_ERROR("mix3: src caps can not be parsed !");
            return FALSE;
        }
        gst_caps_unref(src_caps);
    } else {
        LOG_ERROR("mix3: src caps is NULL !");
        return FALSE;
    }
    //analyze and handle timestamp
    g_mutex_lock(&msg_ctx->lock);
    walk = (GstMessage *) g_queue_peek_head(msg_ctx->message_queue);
    while (walk) {
        root = gst_message_get_structure(walk);
        gst_structure_get_clock_time(root, "timestamp", &msg_pts);
        if (msg_pts > desired) {
            walk = NULL;
            break;
        } else if (msg_pts == desired) {
            break;
        } else {//handle the timestamp unnormal
            if (msg_ctx->delay < desired - msg_pts) {
                msg_ctx->delay = desired - msg_pts;
                LOG_DEBUG("mix3:Delay:%u", msg_ctx->delay);
                MEDIAPIPE_SET_PROPERTY(mp_ret, mp, msg_ctx->delay_queue_name,
                                       "min-threshold-time",
                                       msg_ctx->delay + offset, NULL);
                MEDIAPIPE_SET_PROPERTY(mp_ret, mp, msg_ctx->delay_queue_name,
                                       "max-size-buffers", 0,
                                       NULL);
                MEDIAPIPE_SET_PROPERTY(mp_ret, mp, msg_ctx->delay_queue_name, "max-size-time",
                                       msg_ctx->delay + offset, NULL);
                MEDIAPIPE_SET_PROPERTY(mp_ret, mp, msg_ctx->delay_queue_name, "max-size-bytes",
                                       0, NULL);
            }
            g_queue_pop_head(msg_ctx->message_queue);
            brunchBufferValue = gst_structure_get_value(root, msg_ctx->message_name);
            branchBuffer = (GstBuffer *)g_value_get_pointer(brunchBufferValue);
            gst_buffer_unref(branchBuffer);
            gst_message_unref(walk);
            walk = (GstMessage *) g_queue_peek_head(msg_ctx->message_queue);
        }
    }
    g_mutex_unlock(&msg_ctx->lock);
    //copy roimeta to current buffer from branch buffer
    if (walk != NULL) {
        brunchBufferValue = gst_structure_get_value(root, msg_ctx->message_name);
        branchBuffer = (GstBuffer *)g_value_get_pointer(brunchBufferValue);
        gst_buffer_copy_into(buffer, branchBuffer,
                             GstBufferCopyFlags(GST_BUFFER_COPY_NONE | GST_BUFFER_COPY_META), 0, 0);
        gst_structure_get_uint(root, "orign_width", &branchBufferWidth);
        gst_structure_get_uint(root, "orign_height", &branchBufferHeight);
        change_metainfo_by_width_height(buffer, _width, _height, branchBufferWidth, branchBufferHeight);
        gst_buffer_unref(branchBuffer);
        g_queue_pop_head(msg_ctx->message_queue);
        gst_message_unref(walk);
    }
    return TRUE;
}

static gboolean
change_metainfo_by_width_height(GstBuffer *currentBuffer, guint currentBufferWidth,
                             guint currentBufferHeight, guint branchBufferWidth,
                             guint branchBufferHeight)
{
    RETURN_VAL_IF_FAIL(currentBuffer != NULL, FALSE);
    if ((currentBufferWidth == branchBufferWidth)
        && (currentBufferHeight == branchBufferHeight)) {
        return TRUE;
    }
    GstMeta *gst_meta = NULL;
    gpointer state = NULL;
    while ((gst_meta = gst_buffer_iterate_meta(currentBuffer, &state)) != NULL) {
        if (gst_meta->info->api == GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE) {
            GstVideoRegionOfInterestMeta *meta = (GstVideoRegionOfInterestMeta *)gst_meta;
            meta->x = (guint)(meta->x*((gdouble)(currentBufferWidth)/branchBufferWidth));
            meta->y = (guint)(meta->y*((gdouble)(currentBufferHeight)/branchBufferHeight));
            meta->w = (guint)(meta->w*((gdouble)(currentBufferWidth)/branchBufferWidth));
            meta->h = (guint)(meta->h*((gdouble)(currentBufferHeight)/branchBufferHeight));
        }
    }
    return TRUE;
}

static gboolean
json_analyse_and_post_message(mediapipe_t *mp, const gchar *elem_name)
{
    //find mix3 json object
    struct json_object *mix_root;
    struct json_object *mix_object;
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(mp->config, elem_name,
                       &mix_root), FALSE);
    mix3_ctx *ctx = (mix3_ctx *) mp_modules_find_module_ctx(mp, "mix3");
    //init message process fun hash table
    g_hash_table_insert(ctx->msg_pro_fun_hst, g_strdup("tracking"),
                        (gpointer) process_tracking_message);

    //analyze config , post message , add callback
    struct json_object *subscrib_message_array = NULL;
    struct json_object *color_array = NULL;
    struct json_object *message_obj = NULL;
    const char *message_name;
    const char *delay_queue_name;
    const char *element_name;
    gboolean find = FALSE;
    guint  mix_num_values = json_object_array_length(mix_root);
    for (guint z = 0; z < mix_num_values; z++) {
        mix_object = json_object_array_get_idx(mix_root, z);
        //judge if has specific element named element_name
        if (!json_get_string(mix_object, "element_name", &element_name)) {
            LOG_WARNING("there is no element_name in json  \"%s\"", element_name);
            continue;
        }
        GstElement *element = gst_bin_get_by_name(GST_BIN(mp->pipeline), element_name);
        if (!element) {
            LOG_WARNING("Add callback failed, can not find element \"%s\"", element_name);
            continue;
        }
        gst_object_unref(element);
        //combine message info
        if (json_object_object_get_ex(mix_object, "subscribe_message", &subscrib_message_array)) {
            if (!json_get_string(mix_object, "delay_queue", &delay_queue_name)) {
                delay_queue_name = "mix3_queue";
            }
            guint  num_values = json_object_array_length(subscrib_message_array);
            for (guint i = 0; i < num_values; i++) {
                message_obj = json_object_array_get_idx(subscrib_message_array, i);
                message_name = json_object_get_string(message_obj);
                gpointer  fun = g_hash_table_lookup(ctx->msg_pro_fun_hst, message_name);
                if (NULL != fun) {
                    find = TRUE;
                    ctx->msg_ctxs[ctx->msg_ctx_num].elem_name = element_name;
                    ctx->msg_ctxs[ctx->msg_ctx_num].message_name = message_name;
                    ctx->msg_ctxs[ctx->msg_ctx_num].pro_fun = fun;
                    ctx->msg_ctxs[ctx->msg_ctx_num].message_queue = g_queue_new();
                    ctx->msg_ctxs[ctx->msg_ctx_num].delay_queue_name = delay_queue_name;
                    g_mutex_init(&ctx->msg_ctxs[ctx->msg_ctx_num].lock);
                    ctx->msg_ctx_num++;
                    GstMessage *m;
                    GstStructure *s;
                    GstBus *bus = gst_element_get_bus(mp->pipeline);
                    s = gst_structure_new("subscribe_message",
                                          "message_name", G_TYPE_STRING, message_name,
                                          "subscriber_name", G_TYPE_STRING, element_name,
                                          "message_process_fun", G_TYPE_POINTER, fun,
                                          NULL);
                    m = gst_message_new_application(NULL, s);
                    LOG_DEBUG("mix3: message_name: %s", message_name);
                    LOG_DEBUG("mix3: element_name: %s", element_name);
                    LOG_DEBUG("mix3: delay_queue_name: %s", delay_queue_name);
                    gst_bus_post(bus, m);
                    gst_object_unref(bus);
                }
            }
            if (json_object_object_get_ex(mix_object, "draw_color", &color_array)) {
                struct json_object *color_chanel_array = NULL;
                guint color_num_values = json_object_array_length(color_array);
                for (guint i = 0; i < num_values && i < color_num_values; i++) {
                    color_chanel_array = json_object_array_get_idx(color_array, i);
                    int json_length = json_object_array_length(color_chanel_array);
                    for (int y = 0; y < 3 && y < json_length; y++) {
                        ctx->msg_ctxs[z*num_values + i].draw_color_channel[y] =
                            json_object_get_int(json_object_array_get_idx(color_chanel_array, y));
                    }
                }
            }
            if (find) {
                mediapipe_set_user_callback(mp, element_name, "sink", mix3_src_callback,
                                            (void *)element_name);
            } else {
                continue;
            }
        }
    }
    return TRUE;
}

static gboolean
mix3_src_callback(mediapipe_t *mp, GstBuffer *buf, guint8 *data, gsize size,
                  gpointer user_data)
{
    const char *name = (const char *) user_data;
    mix3_ctx *ctx = (mix3_ctx *) mp_modules_find_module_ctx(mp, "mix3");
    guint i;
    for (i = 0; i < ctx->msg_ctx_num; i++) {
        if (0 == g_strcmp0(ctx->msg_ctxs[i].elem_name, name)) {
            draw_buffer_by_message(mp, &ctx->msg_ctxs[i],
                                   buf, name);
        }
    }
    return TRUE;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis  use pango cairo draw a text and blend into stream frame
 *            alse can use cairo draw rectangle and other things if needed
 *
 * @Param buffer stream frame buffer
 * @Param info  stream frame info
 * @Param x     the text x pos in the frame
 * @Param y     the text y pos in the frame
 * @Param width  the width of the text
 * @Param height the height of the text
 * @Param text
 *
 * @Returns   if success return true
 */
/* ----------------------------------------------------------------------------*/
#if SWITCHON
static gboolean
mix3_draw_text(GstBuffer *buffer, GstVideoInfo *info, int x, int y,
               int width, int height, const gchar *text)
{
    cairo_render_t *render = cairo_render_create(width,  height, "Sans 14");
    gchar *data = cairo_render_get_rgba_data(render,  text);
    GstBuffer *text_buf = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY,
                          data, 4 * width * height, 0, 4 * width * height, NULL, NULL);
    GstVideoFrame frame;
    GstMapFlags mapFlag = GstMapFlags(GST_MAP_READ | GST_MAP_WRITE);
    buffer = gst_buffer_make_writable(buffer);
    if (!gst_video_frame_map(&frame, info, buffer, mapFlag)) {
        LOG_ERROR("draw text video frame map error1");
        return FALSE;
    }
    GstVideoFrame text_frame;
    GstVideoInfo text_info;
    char caps_string[100];
    sprintf(caps_string, "video/x-raw,format=BGRA,width=%d,height=%d", width,
            height);
    GstCaps *caps = gst_caps_from_string(caps_string);
    gst_video_info_from_caps(&text_info, caps);
    gst_caps_unref(caps);
    if (!gst_video_frame_map(&text_frame, &text_info, text_buf, GST_MAP_READ)) {
        LOG_ERROR("draw text video frame map error2");
        return FALSE;
    }
    gboolean ret =  gst_video_blend(&frame, &text_frame, x, y, 1.0);
    gst_video_frame_unmap(&frame);
    gst_video_frame_unmap(&text_frame);
    cairo_render_destroy(render);
    gst_buffer_unref(text_buf);
    return ret;
}
#endif

static void *create_ctx(mediapipe_t *mp)
{
    mix3_ctx *ctx = g_new0(mix3_ctx, 1);
    if (!ctx) {
        return NULL;
    }
    ctx->msg_pro_fun_hst = g_hash_table_new_full(g_str_hash, g_str_equal,
                           (GDestroyNotify)g_free, NULL);
    return ctx;
}

static void destroy_ctx(void *_ctx)
{
    mix3_ctx *ctx = (mix3_ctx *)_ctx;
    for (guint i = 0; i < ctx->msg_ctx_num; i++) {
        g_queue_free_full(ctx->msg_ctxs[i].message_queue,
                          (GDestroyNotify)gst_message_unref);
        g_mutex_clear(&ctx->msg_ctxs[i].lock);
    }
    if (ctx->msg_pro_fun_hst) {
        g_hash_table_remove_all(ctx->msg_pro_fun_hst);
        g_hash_table_unref(ctx->msg_pro_fun_hst);
    }
    g_free(ctx);
}
