/* * MIT License
 *
 * Copyright (c) 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "mediapipe_com.h"
#include "font.h"

#include "gstocl/oclcommon.h"
#define ONE_BILLON_NANO_SECONDS 1000000000

typedef enum {
    DEFAULT_TRACK_ID = 0,
    WF_FD_TRACK_ID,
    WF_MD_TRACK_ID,
    WF_PD_TRACK_ID,
    OSD_CLOCK_TRACK_ID,
    OSD_CPU_RATE_TRACK_ID,
    OSD_GPU_RATE_TRACK_ID,
    OSD_FLOW_TEXT_TRACK_ID
} oclmix_tracking_id_t;

typedef struct {
    GstClockTime    t;
    struct timespec ts;
} time_stamp_t;

static time_stamp_t osd_timestamp_ctx;

#define MESSAGE_CTX_MAX_NUM 10
typedef struct {
    const char *elem_name;
    const char *message_name;
    void *pro_fun;
    GQueue *message_queue;
    GMutex lock;
    guint delay;
    const char* delay_queue_name;
    guint elem_video_width;
    guint elem_video_height;
} cvsdk_message_ctx;

typedef struct {
    GHashTable *msg_pro_fun_hst;
    cvsdk_message_ctx msg_ctxs[MESSAGE_CTX_MAX_NUM];
    guint msg_ctx_num;
} mix_ctx;


static mix_ctx ctx = {0};

static gboolean
mix_sink_callback(mediapipe_t *mp, GstBuffer *buffer, guint8 *data, gsize size,
                  gpointer user_data);

static mp_int_t
setup_subscribe_message(mediapipe_t *mp, struct json_object *mix_root,
                        const char *elem_name);

static GstMessage *
query_message_about_buffer(mediapipe_t *mp, cvsdk_message_ctx *msg_ctx,
                            GstBuffer *buffer);

static GList *
create_mix_param_list_from_message(GstMessage *walk,
                                   cvsdk_message_ctx *mgs_ctx);

static gpointer
create_logo_param(struct json_object *object);

static gpointer
create_mask_param(struct json_object *object);

static gpointer
create_mosaic_param(struct json_object *object);

static gpointer
create_osd_param(struct json_object *object);

static gpointer
create_wireframe_param(struct json_object *object);

static mp_int_t
queue_message_from_observer(const char *message_name,
                            const char *subscribe_name, GstMessage *message);

static mp_int_t
process_face_message(const char *message_name,
                     const  char *subscribe_name, GstMessage *message);

static mp_int_t
process_motion_message(char *message_name,
                       char *subscribe_name, GstMessage *message);

static mp_int_t
process_ice_message(char *message_name,
                    char *subscribe_name, GstMessage *message);

static mp_int_t
setup_subscribe_message(mediapipe_t *mp, struct json_object *mix_root,
                        const char *elem_name);

static gchar *
get_osd_timestamp_string();

static gboolean
init_osd_rgba_data(OclMixParam *param, guint num, struct json_object *object);

static gboolean
json_setup_oclmix_element(mediapipe_t *mp, const gchar *elem_name);


static gboolean
osd_clock_update(OclMixParam *param, GstClockTime pts);

static gboolean
osd_cpu_rate_update(OclMixParam *param);

static gboolean
osd_flow_text_update(OclMixParam *param);

static gboolean
osd_gpu_rate_update(OclMixParam *param);

static void
osd_meta_process(gpointer data, gpointer user_data);

static void
osd_timestamp_context_prepare(GstElement *pipeline);

static GstPadProbeReturn
osd_timestamp_ctx_init_callback(GstPad *pad, GstPadProbeInfo *info,
                                gpointer user_data);

static gboolean
update_osd_meta_list(GList *osd_list, GstClockTime pts);

static char *
mp_mix_block(mediapipe_t *mp, mp_command_t *cmd);

static void
exit_master(void);

static gpointer g_font_library[] = { (gpointer) font, (gpointer) time_font };

static mp_command_t  mp_mix_commands[] = {
    {
        mp_string("mix"),
        MP_MAIN_CONF,
        mp_mix_block,
        0,
        0,
        NULL
    },
    mp_null_command
};


static mp_core_module_t  mp_mix_module_ctx = {
    mp_string("mix"),
    NULL,
    NULL
};


mp_module_t  mp_mix_module = {
    MP_MODULE_V1,
    &mp_mix_module_ctx,                /* module context */
    mp_mix_commands,                   /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                              /* init master */
    NULL,                              /* init module */
    NULL,                              /* keyshot_process*/
    NULL,                              /* message_process */
    NULL,                              /* init_callback */
    NULL,                              /* netcommand_process */
    exit_master,                       /* exit master */
    MP_MODULE_V1_PADDING
};


static gpointer
create_logo_param(struct json_object *object)
{
    const char *filename;
    guint x, y, width, height;
    RETURN_VAL_IF_FAIL(json_get_string(object, "filename", &filename), NULL);
    char *rgba_data = read_file(filename);
    RETURN_VAL_IF_FAIL(rgba_data != NULL, NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "x", &x), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "y", &y), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "width", &width), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "height", &height), NULL);
    RETURN_VAL_IF_FAIL(width != 0 && height != 0, NULL);
    OclMixParam *param = ocl_mix_meta_new();
    param->rect.x = x;
    param->rect.y = y;
    param->rect.width = width;
    param->rect.height = height;
    param->raw_data = rgba_data;
    param->flag = OCL_MIX_LOGO;
    return (gpointer) param;
    return NULL;
}

static gpointer
create_mask_param(struct json_object *object)
{
    guint32 color;
    guint x, y, width, height;
    RETURN_VAL_IF_FAIL(json_get_uint(object, "x", &x), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "y", &y), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "width", &width), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "height", &height), NULL);
    RETURN_VAL_IF_FAIL(json_get_rgba(object, "color_rgba", &color), NULL);
    RETURN_VAL_IF_FAIL(width != 0 && height != 0, NULL);
    OclMixParam *param = ocl_mix_meta_new();
    param->rect.x = x;
    param->rect.y = y;
    param->rect.width = width;
    param->rect.height = height;
    param->raw_data = fakebuff_create(color, width, height);
    param->flag = OCL_MIX_MASK;
    return (gpointer) param;
    return NULL;
}


static gchar *
get_osd_timestamp_string()
{
    struct timespec ts;
    static gchar clock_string[32];
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm *ct = localtime(&ts.tv_sec);
    g_assert(ct != NULL);
    snprintf(clock_string, 32, "%04d-%02d-%02d %02d:%02d:%02d", ct->tm_year + 1900,
             ct->tm_mon + 1, ct->tm_mday, ct->tm_hour, ct->tm_min, ct->tm_sec);
    return clock_string;
}

static gboolean
init_osd_rgba_data(OclMixParam *param, guint num, struct json_object *object)
{
    const char *text = "Null Text";
    const char *font_desc = "Sans 32";

    if (num) {
        param->raw_data = (gchar *) &g_font_library[0];
        param->array = g_array_sized_new(FALSE, TRUE, sizeof(guint), num);

        for (guint index = 0; index < num; ++index) {
            g_array_append_val(param->array, index);
        }

        return TRUE;
    }

    if (param->track_id == (guint32) OSD_CLOCK_TRACK_ID) {
        text = get_osd_timestamp_string();
    } else {
        json_get_string(object, "text", &text);
    }

    guint num_characters = strlen(text);
    json_get_uint(object, "max-stride", &num_characters);
    json_get_string(object, "font", &font_desc);
    cairo_render_get_suggest_font_size(font_desc, &param->rect.width,
                                       &param->rect.height);
    param->rect.width = param->rect.width * num_characters;
    cairo_render_t *render = cairo_render_create(param->rect.width, param->rect.height,
                          font_desc);
    g_caior_render_list = g_list_append(g_caior_render_list, render);
    param->user_data[0] = (gpointer) render;
    param->raw_data = cairo_render_get_rgba_data(render, text);
    return TRUE;
}

static gpointer
create_osd_param(struct json_object *object)
{
    guint x, y, num = 0;
    const char *name = NULL;
    RETURN_VAL_IF_FAIL(json_get_uint(object, "x", &x), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "y", &y), NULL);
    json_get_uint(object, "num", &num);
    json_get_string(object, "name", &name);
    RETURN_VAL_IF_FAIL(num != 0 || name != NULL, NULL);
    OclMixParam *param = ocl_mix_meta_new();
    param->rect.x = x;
    param->rect.y = y;
    param->rect.width = FONT_BLOCK_SIZE;
    param->rect.height = FONT_BLOCK_SIZE;
    param->flag = OCL_MIX_OSD;

    if (name == NULL) {
        param->track_id = (guint32) DEFAULT_TRACK_ID;
    } else if (!strcmp(name, "clock")) {
        param->track_id = (guint32) OSD_CLOCK_TRACK_ID;
    } else if (!strcmp(name, "cpu")) {
        param->track_id = (guint32) OSD_CPU_RATE_TRACK_ID;
    } else if (!strcmp(name, "gpu")) {
        param->track_id = (guint32) OSD_GPU_RATE_TRACK_ID;
    } else if (!strcmp(name, "flow_text")) {
        param->track_id = (guint32) OSD_FLOW_TEXT_TRACK_ID;
    }

    if (!init_osd_rgba_data(param, num, object)) {
        g_free(param);
        return NULL;
    }

    return (gpointer) param;
    return NULL;
}

static gpointer
create_mosaic_param(struct json_object *object)
{
    guint x, y, width, height;
    RETURN_VAL_IF_FAIL(json_get_uint(object, "x", &x), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "y", &y), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "width", &width), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "height", &height), NULL);
    RETURN_VAL_IF_FAIL(width != 0 && height != 0, NULL);
    OclMixParam *param = ocl_mix_meta_new();
    param->rect.x = x;
    param->rect.y = y;
    param->rect.width = width;
    param->rect.height = height;
    param->flag = OCL_MIX_MOSAIC;
    return (gpointer) param;
    return NULL;
}

static gpointer
create_wireframe_param(struct json_object *object)
{
    guint x, y, width, height;
    RETURN_VAL_IF_FAIL(json_get_uint(object, "x", &x), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "y", &y), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "width", &width), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "height", &height), NULL);
    RETURN_VAL_IF_FAIL(width != 0 && height != 0, NULL);
    OclMixParam *param = ocl_mix_meta_new();
    param->rect.x = x;
    param->rect.y = y;
    param->rect.width = width;
    param->rect.height = height;
    param->flag = OCL_MIX_WIREFRAME;
    return (gpointer) param;
    return NULL;
}

static void
osd_meta_process(gpointer data, gpointer user_data)
{
    OclMixParam *param = (OclMixParam *) data;
    GstClockTime pts = *((GstClockTime *) user_data);
    oclmix_tracking_id_t track_id = (oclmix_tracking_id_t) param->track_id;

    switch (track_id) {
    case OSD_CLOCK_TRACK_ID:
        osd_clock_update(param, pts);
        break;

    case OSD_CPU_RATE_TRACK_ID:
        osd_cpu_rate_update(param);
        break;

    case OSD_GPU_RATE_TRACK_ID:
        osd_gpu_rate_update(param);
        break;

    case OSD_FLOW_TEXT_TRACK_ID:
        osd_flow_text_update(param);
        break;

    default:
        break;
    }
}

static gboolean
update_osd_meta_list(GList *osd_list, GstClockTime pts)
{
    RETURN_VAL_IF_FAIL(osd_list != NULL, FALSE);
    g_list_foreach(osd_list, osd_meta_process, (gpointer) &pts);
    return TRUE;
}

static gboolean
json_setup_oclmix_element(mediapipe_t *mp, const gchar *elem_name)
{
    GList *logo_list, *mask_list, *osd_list, *mosaic_list, *wireframe_list;
    struct json_object *mix_root;
    RETURN_VAL_IF_FAIL(mp != NULL, FALSE);
    RETURN_VAL_IF_FAIL(mp->config != NULL, FALSE);
    RETURN_VAL_IF_FAIL(elem_name != NULL, FALSE);
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(mp->config, elem_name,
                   &mix_root), FALSE);
    GstElement *element = gst_bin_get_by_name(GST_BIN(mp->pipeline), elem_name);

    if (!element) {
        LOG_WARNING("Add callback failed, can not find element \"%s\"", elem_name);
        return FALSE;
    }

    if (MP_OK == setup_subscribe_message(mp, mix_root, elem_name)) {
        mediapipe_set_user_callback(mp, elem_name, "sink", mix_sink_callback,
                (void *)elem_name);
    }
    logo_list = json_parse_config(mix_root, "logo", create_logo_param);

    if (logo_list != NULL) {
        ocl_mix_meta_list_append(element, logo_list, OCL_MIX_LOGO);
    }

    mask_list = json_parse_config(mix_root, "mask", create_mask_param);

    if (mask_list != NULL) {
        ocl_mix_meta_list_append(element, mask_list, OCL_MIX_MASK);
    }

    osd_list = json_parse_config(mix_root, "osd", create_osd_param);

    if (osd_list != NULL) {
        ocl_mix_meta_list_append(element, osd_list, OCL_MIX_OSD);
    }

    mosaic_list = json_parse_config(mix_root, "mosaic", create_mosaic_param);

    if (mosaic_list != NULL) {
        ocl_mix_meta_list_append(element, mosaic_list, OCL_MIX_MOSAIC);
    }

    wireframe_list = json_parse_config(mix_root, "wf", create_wireframe_param);

    if (wireframe_list != NULL) {
        ocl_mix_meta_list_append(element, wireframe_list, OCL_MIX_WIREFRAME);
    }

    osd_timestamp_context_prepare(mp->pipeline);
    ocl_mix_set_osd_meta_update_callback(element, update_osd_meta_list);
    gst_object_unref(element);
    return TRUE;
}


static GstPadProbeReturn
osd_timestamp_ctx_init_callback(GstPad *pad, GstPadProbeInfo *info,
                                gpointer user_data)
{
    osd_timestamp_ctx.t = GST_BUFFER_PTS(GST_PAD_PROBE_INFO_BUFFER(info));
    clock_gettime(CLOCK_REALTIME, &osd_timestamp_ctx.ts);
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    return GST_PAD_PROBE_REMOVE;
}

static void
osd_timestamp_context_prepare(GstElement *pipeline)
{
    GstElement *camera = gst_bin_get_by_name(GST_BIN(pipeline), "src");
    GstPad *pad = gst_element_get_static_pad(camera, "src");

    if (pad) {
        gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                          osd_timestamp_ctx_init_callback, NULL, NULL);
        gst_object_unref(pad);
    }

    gst_object_unref(camera);
}

static gboolean
osd_clock_update(OclMixParam *param, GstClockTime pts)
{
    RETURN_VAL_IF_FAIL(param != NULL, FALSE);
    RETURN_VAL_IF_FAIL(param->user_data[0] != NULL, FALSE);
    GstClockTimeDiff diff = pts - osd_timestamp_ctx.t;
    RETURN_VAL_IF_FAIL(diff > 0, FALSE);
    struct timespec delta;
    GST_TIME_TO_TIMESPEC((GstClockTime) diff, delta);

    if (!delta.tv_sec
        && osd_timestamp_ctx.ts.tv_nsec + delta.tv_nsec < ONE_BILLON_NANO_SECONDS) {
        return FALSE;
    }

    osd_timestamp_ctx.t = pts;
    osd_timestamp_ctx.ts.tv_sec  += delta.tv_sec;
    osd_timestamp_ctx.ts.tv_nsec += delta.tv_nsec;

    if (osd_timestamp_ctx.ts.tv_nsec / ONE_BILLON_NANO_SECONDS) {
        osd_timestamp_ctx.ts.tv_sec  += osd_timestamp_ctx.ts.tv_nsec /
                                        ONE_BILLON_NANO_SECONDS;
        osd_timestamp_ctx.ts.tv_nsec %= ONE_BILLON_NANO_SECONDS;
    }

    struct tm *ct = localtime(&osd_timestamp_ctx.ts.tv_sec);

    g_assert(ct != NULL);

    gchar clock_string[32];

    snprintf(clock_string, 32, "%04d-%02d-%02d %02d:%02d:%02d", ct->tm_year + 1900,
             ct->tm_mon + 1, ct->tm_mday, ct->tm_hour, ct->tm_min, ct->tm_sec);

    cairo_render_t *render = (cairo_render_t *) param->user_data[0];

    param->raw_data = cairo_render_get_rgba_data(render, clock_string);

    param->osd_update = TRUE;

    return TRUE;
}

static gboolean
osd_cpu_rate_update(OclMixParam *param)
{
    char cpu_message[32];
    RETURN_VAL_IF_FAIL(param != NULL, FALSE);
    RETURN_VAL_IF_FAIL(param->user_data[0] != NULL, FALSE);
    static guint cpu_rate = 0;

    if (++cpu_rate > 300) {
        cpu_rate = 0;
    }

    sprintf(cpu_message, "cpu: %02u", cpu_rate);
    cairo_render_t *render = (cairo_render_t *) param->user_data[0];
    param->raw_data = cairo_render_get_rgba_data(render, cpu_message);
    param->osd_update = TRUE;
    return TRUE;
}

static gboolean
osd_gpu_rate_update(OclMixParam *param)
{
    char gpu_message[32];
    RETURN_VAL_IF_FAIL(param != NULL, FALSE);
    RETURN_VAL_IF_FAIL(param->user_data[0] != NULL, FALSE);
    static guint gpu_rate = 0;

    if (++gpu_rate > 300) {
        gpu_rate = 0;
    }

    sprintf(gpu_message, "gpu: %02u", gpu_rate);
    cairo_render_t *render = (cairo_render_t *) param->user_data[0];
    param->raw_data = cairo_render_get_rgba_data(render, gpu_message);
    param->osd_update = TRUE;
    return TRUE;
}

static gboolean
osd_flow_text_update(OclMixParam *param)
{
    RETURN_VAL_IF_FAIL(param != NULL, FALSE);

    if (++param->rect.x > 1900) {
        param->rect.x = 0;
    }

    return TRUE;
}

static char *
mp_mix_block(mediapipe_t *mp, mp_command_t *cmd)
{
    if(mp->config == NULL) {
      return (char*) MP_CONF_ERROR;
    };
    json_object_object_foreach(mp->config, key, val) {
        if(NULL!= strstr(key,"mix")){
            json_setup_oclmix_element(mp, key);
        }
    }
    return MP_CONF_OK;
}

static void
exit_master(void)
{
    for (int i = 0; i < ctx.msg_ctx_num; i++) {
        g_queue_free_full(ctx.msg_ctxs[i].message_queue,
                          (GDestroyNotify)gst_message_unref);
        g_mutex_clear(&ctx.msg_ctxs[i].lock);
    }
    g_hash_table_unref(ctx.msg_pro_fun_hst);
}

static mp_int_t
queue_message_from_observer(const char *message_name,
                     const char *subscribe_name, GstMessage *message)
{
    for (int i = 0; i < MESSAGE_CTX_MAX_NUM; i++) {
        if (0 == g_strcmp0(message_name, ctx.msg_ctxs[i].message_name)
            && 0 == g_strcmp0(subscribe_name, ctx.msg_ctxs[i].elem_name)) {
            g_mutex_lock(&ctx.msg_ctxs[i].lock);
            g_queue_push_tail(ctx.msg_ctxs[i].message_queue,
                              gst_message_ref(message));
            g_mutex_unlock(&ctx.msg_ctxs[i].lock);
        }
    }
    return MP_OK;
}

static mp_int_t
process_face_message(const char *message_name,
                     const char *subscribe_name, GstMessage *message)
{
    return queue_message_from_observer(message_name, subscribe_name,
                                       message);
}

static mp_int_t
process_motion_message(char *message_name,
                       char *subscribe_name, GstMessage *message)
{
    return queue_message_from_observer(message_name, subscribe_name,
                                       message);
}

static mp_int_t
process_icv_message(char *message_name,
                    char *subscribe_name, GstMessage *message)
{
    return queue_message_from_observer(message_name, subscribe_name,
                                       message);
}

static mp_int_t
setup_subscribe_message(mediapipe_t *mp, struct json_object *mix_root,
                        const char *elem_name)
{
    //init message process fun hash table
    if (NULL == ctx.msg_pro_fun_hst) {
        ctx.msg_pro_fun_hst = g_hash_table_new_full(g_str_hash, g_str_equal,
                              (GDestroyNotify)g_free, NULL);
        g_hash_table_insert(ctx.msg_pro_fun_hst, g_strdup("faces"),
                            (gpointer) process_face_message);
        g_hash_table_insert(ctx.msg_pro_fun_hst, g_strdup("motions"),
                            (gpointer) process_motion_message);
        g_hash_table_insert(ctx.msg_pro_fun_hst, g_strdup("icv"),
                            (gpointer) process_icv_message);
    }
    GstElement *element = gst_bin_get_by_name(GST_BIN(mp->pipeline),
                          elem_name);
    if (!element) {
        g_print("mix moudle get element:%s failed\n", elem_name);
        return MP_ERROR;
    }
    GstPad *pad = gst_element_get_static_pad(element, "sink");
    GstCaps *caps = gst_pad_get_allowed_caps(pad);
    gint width = 0, height = 0;
    get_resolution_from_caps(caps, &width, &height);
    if (width <= 0 ||  height <= 0) {
        g_print("mix moudle get sink width and height failed\n");
        gst_object_unref(element);
        gst_object_unref(pad);
        gst_caps_unref(caps);
        return MP_ERROR;
    }
    gst_object_unref(element);
    gst_object_unref(pad);
    gst_caps_unref(caps);
    struct json_object *array = NULL;
    struct json_object *message_obj = NULL;
    const char *message_name;
    const char *delay_queue_name;
    gboolean find = FALSE;
    if (json_object_object_get_ex(mix_root, "subscribe_message", &array)) {
        if (!json_get_string(mix_root, "delay_queue", &delay_queue_name)) {
            delay_queue_name = "queue_mix";
        }
        guint  num_values = json_object_array_length(array);
        for (guint i = 0; i < num_values; i++) {
            message_obj = json_object_array_get_idx(array, i);
            message_name = json_object_get_string(message_obj);
            //gpointer  fun = g_hash_table_lookup(ctx.msg_pro_fun_hst, message_name);
            gpointer  fun = g_hash_table_lookup(ctx.msg_pro_fun_hst, message_name);
            if (NULL != fun) {
                find = TRUE;
                ctx.msg_ctxs[ctx.msg_ctx_num].elem_name = elem_name;
                ctx.msg_ctxs[ctx.msg_ctx_num].message_name = message_name;
                ctx.msg_ctxs[ctx.msg_ctx_num].pro_fun = fun;
                ctx.msg_ctxs[ctx.msg_ctx_num].message_queue = g_queue_new();
                ctx.msg_ctxs[ctx.msg_ctx_num].delay_queue_name = delay_queue_name;
                ctx.msg_ctxs[ctx.msg_ctx_num].elem_video_width = width;
                ctx.msg_ctxs[ctx.msg_ctx_num].elem_video_height = height;
                g_mutex_init(&ctx.msg_ctxs[ctx.msg_ctx_num].lock);
                ctx.msg_ctx_num++;
                //register subscriber to cvsdk
                GstMessage *m;
                GstStructure *s;
                GstBus *bus = gst_element_get_bus(mp->pipeline);
                s = gst_structure_new("subscribe_message",
                                      "message_name", G_TYPE_STRING, message_name,
                                      "subscriber_name", G_TYPE_STRING, elem_name,
                                      "message_process_fun", G_TYPE_POINTER, fun,
                                      NULL);
                m = gst_message_new_application(NULL, s);
                gst_bus_post(bus, m);
                gst_object_unref(bus);
            } else {
                find = find || FALSE;
            }
        }
        if (find) {
            return MP_OK;
        } else {
            return MP_ERROR;
        }
    }
    return MP_ERROR;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis get data from gst_messge and conert to meta , add into mix element
 *
 * @Param mp
 * @Param buffer
 * @Param data
 * @Param size
 * @Param user_data
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static gboolean
mix_sink_callback(mediapipe_t *mp, GstBuffer *buffer, guint8 *data, gsize size,
                  gpointer user_data)
{
    const char *name = (const char *) user_data;
    GstElement *element = gst_bin_get_by_name(GST_BIN(mp->pipeline), name);
    GList *list = NULL;
    GList *l = NULL;
    gint i;
    GstMessage *walk;
    GList *hst_list = (GList *) g_hash_table_get_values(ctx.msg_pro_fun_hst);
    l = hst_list;
    while (l != NULL) {
        ocl_mix_meta_list_remove(element,
                                 GPOINTER_TO_UINT(l->data) + 1024, OCL_MIX_WIREFRAME);
        l = l->next;
    }
    g_list_free(hst_list);
    for (i = 0; i < MESSAGE_CTX_MAX_NUM; i++) {
        if (0 == g_strcmp0(ctx.msg_ctxs[i].elem_name, name)) {
            walk = query_message_about_buffer(mp, &ctx.msg_ctxs[i],
                                              buffer);
            list =  create_mix_param_list_from_message(walk, &ctx.msg_ctxs[i]);
            if (list != NULL) {
                ocl_mix_meta_list_append(element, list, OCL_MIX_WIREFRAME);
            }
        }
    }
    gst_object_unref(element);
    return TRUE;
}

static GstMessage *
query_message_about_buffer(mediapipe_t *mp, cvsdk_message_ctx *msg_ctx,
                           GstBuffer *buffer)
{
    GstMessage *walk;
    guint mp_ret = 0;
    GstClockTime msg_pts;
    GstClockTime offset = 300000000;
    GstClockTime desired = GST_BUFFER_PTS(buffer);
    const GstStructure *root, *boxed;
    g_mutex_lock(&msg_ctx->lock);
    walk = (GstMessage *) g_queue_peek_head(msg_ctx->message_queue);
    while (walk) {
        root = gst_message_get_structure(walk);
        gst_structure_get_clock_time(root, "timestamp", &msg_pts);
        if (msg_pts > desired) {
            walk = NULL;
            break;
        } else if (msg_pts == desired) {
            g_queue_pop_head(msg_ctx->message_queue);
            break;
        } else {
            if (msg_ctx->delay < desired - msg_pts) {
                msg_ctx->delay = desired - msg_pts;
                //g_print("===delay=%d",msg_ctx->delay);
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
            gst_message_unref(walk);
            walk = (GstMessage *) g_queue_peek_head(msg_ctx->message_queue);
        }
    }
    g_mutex_unlock(&msg_ctx->lock);
    return walk;
}

static GList *
create_mix_param_list_from_message(GstMessage *walk, cvsdk_message_ctx *msg_ctx)
{
    GList *ret = NULL;
    const GValue *vlist, *item;
    const GstStructure *root, *boxed;
    guint nsize;
    guint x, y, width, height;
    guint i;
    if (walk != NULL) {
        root = gst_message_get_structure(walk);
        vlist = gst_structure_get_value(root, msg_ctx->message_name);
        nsize = gst_value_list_get_size(vlist);
        for (i = 0; i < nsize; ++i) {
            item = gst_value_list_get_value(vlist, i);
            boxed = (GstStructure *) g_value_get_boxed(item);
            if (gst_structure_get_uint(boxed, "x", &x)
                && gst_structure_get_uint(boxed, "y", &y)
                && gst_structure_get_uint(boxed, "width", &width)
                && gst_structure_get_uint(boxed, "height", &height)) {
                OclMixParam *param = ocl_mix_meta_new();
                param->track_id = GPOINTER_TO_UINT(msg_ctx->pro_fun) + 1024;
                //later maybe get the src resolutin , now use the const 1920x1080
                param->rect.x = x * msg_ctx->elem_video_width / 1920;
                param->rect.y = y * msg_ctx->elem_video_height / 1080;
                param->rect.width = width * msg_ctx->elem_video_width / 1920;
                param->rect.height = height * msg_ctx->elem_video_height / 1080;
                param->flag = OCL_MIX_WIREFRAME;
                ret = g_list_append(ret, param);
            }
        }
        gst_message_unref(walk);
    }
    return ret;
}
