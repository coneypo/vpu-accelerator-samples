/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mediapipe_com.h"
#include <string>
#include <algorithm>
#include <vector>

//about subscriber_message start
typedef int (*message_process_fun)(const char *message_name,
                                   const char *subscribe_name,
                                   mediapipe_t *mp, GstMessage *message);

typedef struct {
    const char *message_name;
    const char *subscriber_name;
    message_process_fun fun;
} message_ctx;

static void
subscribe_message(const char *message_name, const char *subscriber_name,
                  mediapipe_t *mp, message_process_fun fun);
static mp_int_t
unsubscribe_message(const char *message_name, const char *subscriber_name,
                    mediapipe_t *mp, message_process_fun fun);

//about subscriber_message end

#define MAX_BUF_SIZE 10240
#define QUEUE_CAPACITY 10

// about branch start
typedef struct {
    mediapipe_branch_t mp_branch;
    gboolean enable;
    const gchar  *pipe_string;
    const gchar  *name;
    const gchar  *src_name;
    const gchar *message_name;
    const gchar *src_format;
    struct json_object *pipe_params;
} openvino_tracking_branch_t;

typedef  openvino_tracking_branch_t branch_t;

typedef struct {
    mediapipe_t *mp;
    GHashTable  *msg_hst;
    gint branch_num;
    openvino_tracking_branch_t *branch;
} openvino_tracking_ctx;

static gboolean
branch_init(mediapipe_branch_t *mp_branch);

static gboolean
branch_config(branch_t *branch);

static void
json_setup_branch(mediapipe_t *mp, struct json_object *root);

static gboolean
push_buffer_to_branch(mediapipe_t *mp, GstBuffer *buffer, guint8 *data,
                      gsize size, gpointer user_data);
// about branch end

//get message and progress it start
static GstPadProbeReturn
detect_src_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);


//get message and progress it end

static  mp_int_t
message_process(mediapipe_t *mp, void *message);

static char *
mp_parse_config(mediapipe_t *mp, mp_command_t *cmd);

static gboolean if_contain_roi_meta(GstBuffer *buffer);

static void *create_ctx(mediapipe_t *mp);
static void destroy_ctx(void *_ctx);

static mp_command_t  mp_openvino_tracking_commands[] = {
    {
        mp_string("openvino_tracking"),
        MP_MAIN_CONF,
        mp_parse_config,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_module_ctx_t  mp_openvino_tracking_module_ctx = {
    mp_string("openvino_tracking"),
    create_ctx,
    NULL,
    destroy_ctx
};

mp_module_t  mp_openvino_tracking_module = {
    MP_MODULE_V1,
    &mp_openvino_tracking_module_ctx,           /* module context */
    mp_openvino_tracking_commands,              /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                              /* init master */
    NULL,                              /* init module */
    NULL,                              /* keyshot_process*/
    message_process,                   /* message_process */
    NULL,                              /* init_callback */
    NULL,                              /* netcommand_process */
    NULL,                              /* exit master */
    MP_MODULE_V1_PADDING
};

//module define end

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis  prase config object from config.json
 *
 * @Param mp mediapipe
 * @Param cmd
 */
/* ----------------------------------------------------------------------------*/
static char *
mp_parse_config(mediapipe_t *mp, mp_command_t *cmd)
{
    json_setup_branch(mp, mp->config);
    return MP_CONF_OK;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis config openvino_tracking branch
 *
 * @Param branch openvino_tracking branch
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static gboolean
branch_config(branch_t *branch)
{
    gchar desc[MAX_BUF_SIZE];
    snprintf(desc, MAX_BUF_SIZE,
             "video/x-raw(memory:DMABuf),format=%s,width=%u,height=%u,framerate=30/1", branch->src_format,
             branch->mp_branch.input_width, branch->mp_branch.input_height);
    GstCaps *caps = gst_caps_from_string(desc);
    guint max_bytes = branch->mp_branch.input_width * branch->mp_branch.input_height
                      * QUEUE_CAPACITY;
    branch->mp_branch.source = gst_bin_get_by_name(GST_BIN(
                                   branch->mp_branch.pipeline), "source");
    if (branch->mp_branch.source == NULL) {
        return FALSE;
    }
    g_object_set(branch->mp_branch.source, "format", GST_FORMAT_TIME, "max_bytes",
                 max_bytes, "caps", caps, NULL);
    gst_caps_unref(caps);
    //add callback to get detect meta
    GstElement *detect = gst_bin_get_by_name(GST_BIN(
                             branch->mp_branch.pipeline), "detect");
    if (detect == NULL) {
        return FALSE;
    }
    GstPad *pad = gst_element_get_static_pad(detect, "src");
    if (pad) {
        gulong probe_id = gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                                            detect_src_callback, branch,
                                            NULL);
        gst_probe_list_append_new_item(branch->mp_branch.probe_items, pad, probe_id);
    }
    gst_object_unref(detect);
    return TRUE;
}

static void create_description_from_string_and_params(branch_t *branch,
        gchar *description)
{
    const gchar *s_p = branch->pipe_string;
    char *d_p = description;
    guint pipe_str_count = 0;
    char param_name[50] = {0};
    const gchar *param_value = NULL;
    char end_char = '\0';
    char *param_p = NULL;
    guint parm_str_count = 0;
    while (pipe_str_count < MAX_BUF_SIZE && s_p && *s_p != '\0') {
        if (*s_p != '$') {
            *d_p = *s_p;
            d_p++;
            s_p++;
            pipe_str_count++;
        } else {
            s_p++;
            if (!s_p) {
                break;
            }
            param_p = &param_name[0];
            parm_str_count = 0;
            switch (*s_p) {
                case '{':
                    end_char = '}';
                    break;
                case '(':
                    end_char = ')';
                    break;
                default:
                    end_char = ' ';
                    *param_p = *s_p;
                    param_p++;
                    parm_str_count++;
                    break;
            }
            s_p++;
            while (parm_str_count < 50 && s_p && *s_p != '\0') {
                if (*s_p != end_char) {
                    *param_p = *s_p;
                    param_p++;
                    s_p++;
                    parm_str_count++;
                } else {
                    if (end_char != ' ') {
                        s_p++;
                    }
                    break;
                }
            }
            *param_p = '\0';
            param_value = NULL;
            if (param_name[0] != '\0' && json_get_string(branch->pipe_params,
                    param_name, &param_value)) {
                int num = g_strlcpy(d_p, param_value,
                                    MAX_BUF_SIZE - pipe_str_count);
                d_p += num;
                pipe_str_count += num;
            }
        }
    }
    *d_p = '\0';
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis openvino_tracking custom branch init
 *
 * @Param mp_branch
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static gboolean
branch_init(mediapipe_branch_t *mp_branch)
{
    g_assert(mp_branch != NULL);
    const gchar *desc_format = NULL;
    gchar description[MAX_BUF_SIZE];
    branch_t *branch = (branch_t *) mp_branch;
    openvino_tracking_ctx *ctx = (openvino_tracking_ctx *)
                                 mp_modules_find_module_ctx(mp_branch->mp, "openvino_tracking");
    create_description_from_string_and_params(branch, description);
    printf("description:[%s]\n", description);
    GstElement *new_pipeline = mediapipe_branch_create_pipeline(description);
    if (new_pipeline == NULL) {
        LOG_ERROR("Failed to create openvino_tracking branch, make sure you have installed \
                openvino_tracking plugins and configured all necessary dependencies");
        return FALSE;
    }
    mp_branch->pipeline = new_pipeline;
    if (!branch_config(branch)) {
        mediapipe_branch_destroy_internal(mp_branch);
        return FALSE;
    }
    return TRUE;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis parse openvino_tracking info from root json object and set up
 *
 * @Param mp mediapipe
 * @Param root root json oject
 */
/* ----------------------------------------------------------------------------*/
static void
json_setup_branch(mediapipe_t *mp, struct json_object *root)
{
    struct json_object *object;
    struct json_object *detect;
    gboolean load_success = FALSE;
    int i = 0;
    RETURN_IF_FAIL(mp != NULL);
    RETURN_IF_FAIL(mp->state != STATE_NOT_CREATE);
    RETURN_IF_FAIL(json_object_object_get_ex(root, "openvino_detection", &object));
    openvino_tracking_ctx *ctx = (openvino_tracking_ctx *)
                                 mp_modules_find_module_ctx(mp, "openvino_tracking");
    int branch_num = json_object_array_length(object);
    ctx->branch_num = branch_num;
    ctx->branch = (branch_t *)g_malloc0(sizeof(branch_t) * branch_num);
    for (int i = 0; i < branch_num; ++i) {
        detect = json_object_array_get_idx(object, i);
        if (json_check_enable_state(detect, "enable")) {
            json_get_string(detect, "src_name",
                            &(ctx->branch[i].src_name));
            json_get_string(detect, "src_format",
                            &(ctx->branch[i].src_format));
            json_get_string(detect, "name",
                            &(ctx->branch[i].name));
            json_get_string(detect, "message_name",
                            &(ctx->branch[i].message_name));
            json_get_string(detect, "pipe_string",
                            &(ctx->branch[i].pipe_string));
            ctx->branch[i].enable = TRUE;
            ctx->branch[i].mp_branch.branch_init = branch_init;
            json_object_object_get_ex(detect, "pipe_params",
                                      &(ctx->branch[i].pipe_params));
            load_success = mediapipe_setup_new_branch(mp, ctx->branch[i].src_name, "src",
                           &ctx->branch[i].mp_branch);
            if (load_success) {
                mediapipe_set_user_callback(mp, ctx->branch[i].src_name, "src",
                                            push_buffer_to_branch,
                                            &ctx->branch[i]);
            }
        }
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis push buffer to openvino_tracking branch
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
push_buffer_to_branch(mediapipe_t *mp, GstBuffer *buffer, guint8 *data,
                      gsize size, gpointer user_data)
{
    branch_t *branch = (branch_t *) user_data;
    return mediapipe_branch_push_buffer(&branch->mp_branch, buffer);
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis subscribe message
 *
 * @Param message_name
 * @Param subscriber_name
 * @Param fun
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static void
subscribe_message(const char *message_name, const char *subscriber_name,
                  mediapipe_t *mp, message_process_fun fun)
{
    openvino_tracking_ctx *ctx = (openvino_tracking_ctx *)
                                 mp_modules_find_module_ctx(mp, "openvino_tracking");
    GList *msg_list = (GList *) g_hash_table_lookup(ctx->msg_hst, message_name);
    GList *l = msg_list;
    message_ctx *t_ctx;
    gboolean find = FALSE;
    while (l != NULL) {
        t_ctx  = (message_ctx *) l->data;
        if (0 == g_strcmp0(t_ctx->message_name, message_name)
            && 0 == g_strcmp0(t_ctx->subscriber_name, subscriber_name)
            && fun ==  t_ctx->fun) {
            find = TRUE;
            break;
        }
        l = l->next;
    }
    if (!find) {
        t_ctx = g_new0(message_ctx, 1);
        t_ctx->message_name = message_name;
        t_ctx->subscriber_name = subscriber_name;
        t_ctx->fun = fun;
        msg_list = g_list_append(msg_list, t_ctx);
        g_hash_table_replace(ctx->msg_hst, g_strdup(message_name), (gpointer)msg_list);
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis unsubscribe message
 *
 * @Param message_name
 * @Param subscriber_name
 * @Param fun
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/

static mp_int_t
unsubscribe_message(const char *message_name, const char *subscriber_name,
                    mediapipe_t *mp, message_process_fun fun)
{
    openvino_tracking_ctx *ctx = (openvino_tracking_ctx *)
                                 mp_modules_find_module_ctx(mp, "openvino_tracking");
    if (NULL == ctx->msg_hst) {
        return MP_ERROR;
    }
    GList *msg_list = (GList *) g_hash_table_lookup(ctx->msg_hst, message_name);
    GList *l = msg_list;
    message_ctx *t_ctx;
    gboolean find = FALSE;
    while (l != NULL) {
        t_ctx  = (message_ctx *) l->data;
        if (0 == g_strcmp0(t_ctx->message_name, message_name)
            && 0 == g_strcmp0(t_ctx->subscriber_name, subscriber_name)
            && fun ==  t_ctx->fun) {
            find = TRUE;
            break;
        }
        l = l->next;
    }
    if (find) {
        msg_list = g_list_remove_link(msg_list, l);
        g_free(l->data);
        g_list_free(l);
        g_hash_table_replace(ctx->msg_hst, g_strdup(message_name), (gpointer)msg_list);
    }
    return MP_OK;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis  module function prgress bus message
 *
 * @Param mp   mediapipe
 * @Param message
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static  mp_int_t
message_process(mediapipe_t *mp, void *message)
{
    GstMessage *m = (GstMessage *) message;
    const  GstStructure *s;
    const gchar *msg_name_s;
    const gchar *subscriber_name;
    void *func = NULL;
    if (GST_MESSAGE_TYPE(m) != GST_MESSAGE_APPLICATION) {
        return MP_IGNORE;
    }
    s = gst_message_get_structure(m);
    const gchar *name = gst_structure_get_name(s);
    if (0 == g_strcmp0(name, "subscribe_message")) {
        if (gst_structure_get(s,
                              "message_name", G_TYPE_STRING, &msg_name_s,
                              "subscriber_name", G_TYPE_STRING, &subscriber_name,
                              "message_process_fun", G_TYPE_POINTER, &func,
                              NULL)) {
            subscribe_message(msg_name_s, subscriber_name, mp, (message_process_fun)func);
        }
        return MP_OK;
    } else if (0 == g_strcmp0(name, "unsubscribe_message")) {
        if (gst_structure_get(s,
                              "message_name", G_TYPE_STRING, &msg_name_s,
                              "subscriber_name", G_TYPE_STRING, &subscriber_name,
                              "message_process_fun", G_TYPE_POINTER, &func,
                              NULL)) {
            unsubscribe_message(msg_name_s, subscriber_name, mp,
                                (message_process_fun) func);
        }
        return MP_OK;
    }
    return MP_IGNORE;
}



static gboolean if_contain_roi_meta(GstBuffer *buffer)
{
    GstMeta *gst_meta = NULL;
    gpointer state = NULL;
    while (gst_meta = gst_buffer_iterate_meta(buffer, &state)) {
        if (gst_meta->info->api == GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE) {
            return TRUE;
        }
    }
    return FALSE;
}
/* --------------------------------------------------------------------------*/
/**
 * @Synopsis  get data from buffer meta
 *
 * @Param pad
 * @Param buffer
 * @Param objectlist store the data
 */
/* ----------------------------------------------------------------------------*/
static gboolean
get_value_from_buffer_tracking(GstPad *pad, GstBuffer *buffer,
                               GValue *objectlist)
{
    GstMeta *gst_meta = NULL;
    gpointer state = NULL;
    GstCaps *caps = gst_pad_get_current_caps(pad);
    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, caps)) {
        LOG_ERROR("get video info form caps falied");
        gst_caps_unref(caps);
        return FALSE;
    }
    gst_caps_unref(caps);
    while (gst_meta = gst_buffer_iterate_meta(buffer, &state)) {
        if (gst_meta->info->api != GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE) {
            continue ;
        }
        GstVideoRegionOfInterestMeta *meta = (GstVideoRegionOfInterestMeta *)gst_meta;
        // maybe need to checkout if it's GVA meta
        int label_id = 0;
        GstStructure *structure = NULL;
        for (GList *l = meta->params; l; l = g_list_next(l)) {
            structure = (GstStructure *) l->data;
            if (gst_structure_has_field(structure, "label_id") &&
                gst_structure_get_int(structure, "label_id", &label_id)) {
                LOG_DEBUG("label_id: %d", label_id);
                break;
            }
        }
        //create content list
        GValue tmp_value = { 0 };
        g_value_init(&tmp_value, GST_TYPE_STRUCTURE);
        GstStructure *s =
            gst_structure_new("object",
                              "x", G_TYPE_UINT, meta->x,
                              "y", G_TYPE_UINT, meta->y,
                              "width", G_TYPE_UINT, meta->w,
                              "height", G_TYPE_UINT, meta->h,
                              "orign_width", G_TYPE_UINT, info.width, //add for mix2 paint Rectangle
                              "orign_height", G_TYPE_UINT, info.height,//add for mix2 paint Rectangle
                              "label_id", G_TYPE_INT, label_id,
                              NULL);
        g_value_take_boxed(&tmp_value, s);
        gst_value_list_append_value(objectlist, &tmp_value);
        g_value_unset(&tmp_value);
        LOG_DEBUG("meta data:%d,%d,%d,%d,%d, %ld", meta->x, meta->y,
                  meta->w, meta->h, label_id,
                  GST_BUFFER_PTS(buffer));
    }
    return TRUE;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis callback function for get detect meta
 *
 * @Param pad
 * @Param info
 * @Param user_data
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static GstPadProbeReturn
detect_src_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    branch_t *branch = (branch_t *) user_data;
    openvino_tracking_ctx *ctx = (openvino_tracking_ctx *)
                                 mp_modules_find_module_ctx(branch->mp_branch.mp, "openvino_tracking");
    if (NULL == ctx->msg_hst) {
        LOG_DEBUG("%s:%d : hash table in NULL", __FILE__, __LINE__);
        return GST_PAD_PROBE_REMOVE;
    }
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    LOG_DEBUG("branch->name:%s", branch->name);
    if (!if_contain_roi_meta(buffer)) {
        return GST_PAD_PROBE_OK;
    }
    //get videoInfo for getting width and height
    GstCaps *caps = gst_pad_get_current_caps(pad);
    GstVideoInfo bufferInfo;
    if (!gst_video_info_from_caps(&bufferInfo, caps)) {
        LOG_ERROR("get video info form caps falied");
        gst_caps_unref(caps);
        return GST_PAD_PROBE_OK;
    }
    gst_caps_unref(caps);

    GValue bufferValue = { 0 };
    g_value_init(&bufferValue, G_TYPE_POINTER);
    g_value_set_pointer(&bufferValue, buffer);
    //create messsage
    GstStructure *s = gst_structure_new(branch->name,
                                        "timestamp", G_TYPE_UINT64,
                                        GST_BUFFER_PTS(buffer),
                                        "orign_width", G_TYPE_UINT, bufferInfo.width,
                                        "orign_height", G_TYPE_UINT, bufferInfo.height,
                                        NULL);
    GstMessage *msg =  gst_message_new_element(NULL, s);
    gst_structure_set_value(
        (GstStructure *) gst_message_get_structure(msg),
        branch->message_name,
        &bufferValue);
    g_value_unset(&bufferValue);
    //let subscribe to progress msg
    GList *msg_pro_list = (GList *) g_hash_table_lookup(ctx->msg_hst,
                          branch->message_name);
    GList *l = NULL;
    if (NULL != msg_pro_list) {
        l = msg_pro_list;
        message_ctx *t_ctx;
        while (l != NULL) {
            t_ctx  = (message_ctx *) l->data;
            gst_buffer_ref(buffer);
            t_ctx->fun(t_ctx->message_name, t_ctx->subscriber_name, branch->mp_branch.mp,
                       msg);
            l = l->next;
        }
    }
    gst_message_unref(msg);
    return GST_PAD_PROBE_OK;
}

static void *create_ctx(mediapipe_t *mp)
{
    openvino_tracking_ctx *ctx = g_new0(openvino_tracking_ctx, 1);
    if (!ctx) {
        return NULL;
    }
    ctx->mp = mp;
    ctx->msg_hst = g_hash_table_new_full(g_str_hash, g_str_equal,
                                         (GDestroyNotify)g_free, NULL);
    return ctx;
}

static void destroy_ctx(void *_ctx)
{
    openvino_tracking_ctx *ctx = (openvino_tracking_ctx *)_ctx;
    if (!ctx) {
        return;
    }
    for (gint i = 0; i < ctx->branch_num; i++) {
        if (ctx->branch[i].enable) {
            mediapipe_branch_destroy_internal(&ctx->branch[i].mp_branch);
        }
    }
    if (ctx->msg_hst) {
        GList *l = g_hash_table_get_values(ctx->msg_hst);
        GList *p = l;
        while (p != NULL) {
            g_list_free_full((GList *)p->data, (GDestroyNotify)g_free);
            p = p->next;
        }
        g_list_free(l);
        g_hash_table_remove_all(ctx->msg_hst);
        g_hash_table_unref(ctx->msg_hst);
    }
    g_free(ctx->branch);
    g_free(ctx);
}
