/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mediapipe_com.h"

#define CVSDK_BASE_TRACKID 1024
#define MAX_BUF_SIZE 512
#define QUEUE_CAPACITY 10
#define DEFAULT_FD_HETEROGENEITY 7

typedef enum {
    UNKNOWN_BRANCH_TYPE = -1,
    CVSDK_MD_BRANCH_TYPE = 0,
    CVSDK_FD_BRANCH_TYPE,
    ICV_PD_BRANCH_TYPE,
    BRANCH_TYPE_MAX_NUM
} mediapipe_branch_type_t;

typedef struct {
    mediapipe_branch_t mp_branch;
    mediapipe_branch_type_t branch_type;
    gboolean branch_enable;
    guint  scale_w;
    guint  scale_h;
    GQueue *smt_queue;
    GMutex lock;
    char  *format;
    guint timeouts;
    const gchar *root_name;
    const gchar *item_name;
    GstClockTime min_pts;
} cvsdk_branch_t;

typedef int (*message_process_fun)(const char *message_name,
                                   const char *subscribe_name, GstMessage *message);

typedef struct {
    const char *message_name;
    const char *subscriber_name;
    message_process_fun fun;
} message_ctx;

typedef struct {
    cvsdk_branch_t cv_branch[BRANCH_TYPE_MAX_NUM];
    GstClockTime delay;
    mediapipe_t *mp;
    GHashTable  *msg_hst;
} cvsdk_ctx_t;

static mp_int_t
subscribe_message(const char *message_name,
                  const char *subscriber_name, message_process_fun fun);
static mp_int_t
unsubscribe_message(const char *message_name,
                    const char *subscriber_name, message_process_fun fun);


static mp_int_t
init_callback(mediapipe_t *mp);

static gboolean
branch_init(mediapipe_branch_t *mp_branch);

static gboolean
cvsdk_branch_bus_callback(GstBus *bus, GstMessage *message, gpointer data);

static gboolean
cvsdk_branch_config(cvsdk_branch_t *branch);

static void
json_setup_cvsdk_branch(mediapipe_t *mp, struct json_object *root);

static char *
mp_cvsdk_block(mediapipe_t *mp, mp_command_t *cmd);

static gboolean
push_buffer_to_cvsdk_branch(mediapipe_t *mp, GstBuffer *buffer, guint8 *data,
                            gsize size, gpointer user_data);


static void
exit_master(void);

static  mp_int_t
message_process(mediapipe_t *mp, void *message);

static gboolean
mix_sink_callback(mediapipe_t *mp, GstBuffer *buffer, guint8 *data, gsize size,
                  gpointer user_data);

static GList *
query_cvsdk_udpate_branch_result(cvsdk_branch_t *branch, GstBuffer *buffer);

static cvsdk_ctx_t ctx= {0};


static mp_command_t  mp_cvsdk_commands[] = {
    {
        mp_string("cvsdk"),
        MP_MAIN_CONF,
        mp_cvsdk_block,
        0,
        0,
        NULL
    },
    mp_null_command
};


static mp_core_module_t  mp_cvsdk_module_ctx = {
    mp_string("cvsdk"),
    NULL,
    NULL
};


mp_module_t  mp_cvsdk_module = {
    MP_MODULE_V1,
    &mp_cvsdk_module_ctx,              /* module context */
    mp_cvsdk_commands,                 /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                              /* init master */
    NULL,                              /* init module */
    NULL,                              /* keyshot_process*/
    message_process,                              /* message_process */
    init_callback,                              /* init_callback */
    NULL,                              /* netcommand_process */
    exit_master,                       /* exit master */
    MP_MODULE_V1_PADDING
};


const char *cvsdk_face_detection_heterogeneity_config_file = NULL;

static char *
mp_cvsdk_block(mediapipe_t *mp, mp_command_t *cmd)
{
    memset(&ctx, 0, sizeof(cvsdk_ctx_t));
    ctx.mp = mp;
    ctx.cv_branch[CVSDK_MD_BRANCH_TYPE].branch_type = CVSDK_MD_BRANCH_TYPE;
    ctx.cv_branch[CVSDK_MD_BRANCH_TYPE].mp_branch.branch_init = branch_init;
    ctx.cv_branch[CVSDK_MD_BRANCH_TYPE].scale_w = 640;
    ctx.cv_branch[CVSDK_MD_BRANCH_TYPE].scale_h = 360;
    ctx.cv_branch[CVSDK_MD_BRANCH_TYPE].format = (char*)
        "appsrc name=source ! queue max-size-buffers=1 leaky=2 !  \
        oclscale ! video/x-raw,format=NV12,width=%u,height=%u,tiled=false ! \
        cvsdk-motiondetection heterogeneity=4 post-msg=true draw-rois=false ! fakesink";
    ctx.cv_branch[CVSDK_MD_BRANCH_TYPE].root_name = "cvsdk-motiondetection";
    ctx.cv_branch[CVSDK_MD_BRANCH_TYPE].item_name = "motions";
    ctx.cv_branch[CVSDK_MD_BRANCH_TYPE].smt_queue = g_queue_new();
    g_queue_init(ctx.cv_branch[CVSDK_MD_BRANCH_TYPE].smt_queue);
    g_mutex_init(&ctx.cv_branch[CVSDK_MD_BRANCH_TYPE].lock);
    ctx.cv_branch[CVSDK_FD_BRANCH_TYPE].branch_type = CVSDK_FD_BRANCH_TYPE;
    ctx.cv_branch[CVSDK_FD_BRANCH_TYPE].mp_branch.branch_init = branch_init;
    ctx.cv_branch[CVSDK_FD_BRANCH_TYPE].scale_w = 640;
    ctx.cv_branch[CVSDK_FD_BRANCH_TYPE].scale_h = 360;
    ctx.cv_branch[CVSDK_FD_BRANCH_TYPE].format = (char*)
        "appsrc name=source ! queue max-size-buffers=1 leaky=2 ! oclscale !  \
        video/x-raw,format=NV12,width=%u,height=%u,tiled=false !  \
        cvsdk-facedetection name=fdetector skip-interval=1 min-face-size=32 post-msg=true draw-rois=false ! fakesink";
    ctx.cv_branch[CVSDK_FD_BRANCH_TYPE].root_name = "cvsdk-facedetection";
    ctx.cv_branch[CVSDK_FD_BRANCH_TYPE].item_name = "faces";
    ctx.cv_branch[CVSDK_FD_BRANCH_TYPE].smt_queue = g_queue_new();
    g_queue_init(ctx.cv_branch[CVSDK_FD_BRANCH_TYPE].smt_queue);
    g_mutex_init(&ctx.cv_branch[CVSDK_FD_BRANCH_TYPE].lock);
    ctx.cv_branch[ICV_PD_BRANCH_TYPE].branch_type = ICV_PD_BRANCH_TYPE;
    ctx.cv_branch[ICV_PD_BRANCH_TYPE].mp_branch.branch_init = branch_init;
    ctx.cv_branch[ICV_PD_BRANCH_TYPE].scale_w = 544;
    ctx.cv_branch[ICV_PD_BRANCH_TYPE].scale_h = 320;
    ctx.cv_branch[ICV_PD_BRANCH_TYPE].format = (char*)
        "appsrc name=source ! queue max-size-buffers=1 leaky=2 ! oclscale ! \
        video/x-raw,format=xBGR,width=%u,height=%u,tiled=true,ignore-alignment=true ! \
        icv-peopledetection name=vdetector post-msg=true ! fakesink";
    ctx.cv_branch[ICV_PD_BRANCH_TYPE].root_name = "icv-peopledetection";
    ctx.cv_branch[ICV_PD_BRANCH_TYPE].item_name = "faces";
    ctx.cv_branch[ICV_PD_BRANCH_TYPE].smt_queue = g_queue_new();
    g_queue_init(ctx.cv_branch[ICV_PD_BRANCH_TYPE].smt_queue);
    g_mutex_init(&ctx.cv_branch[ICV_PD_BRANCH_TYPE].lock);
    json_setup_cvsdk_branch(mp, mp->config);
    return MP_CONF_OK;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis push get message to queue
 *
 * @Param bus
 * @Param message
 * @Param data
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static gboolean
cvsdk_branch_bus_callback(GstBus *bus, GstMessage *message, gpointer data)
{
    const GstStructure *root;
    GstStructure *boxed;
    const GValue *vlist, *item;
    GList *msg_pro_list;
    guint nsize, x, y, width, height;
    GList *l;
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ELEMENT) {
        cvsdk_branch_t *branch = (cvsdk_branch_t *) data;
        if (NULL == ctx.msg_hst) {
            return TRUE;
        }
        msg_pro_list = (GList *) g_hash_table_lookup(ctx.msg_hst, branch->item_name);
        if (NULL != msg_pro_list) {
            //scale widht and height to origin width and height
            root = gst_message_get_structure(message);
            vlist = gst_structure_get_value(root, branch->item_name);
            nsize = gst_value_list_get_size(vlist);
            for (int i = 0; i < nsize; ++i) {
                item = gst_value_list_get_value(vlist, i);
                boxed = (GstStructure *) g_value_get_boxed(item);
                if (gst_structure_get_uint(boxed, "x", &x)
                    && gst_structure_get_uint(boxed, "y", &y)
                    && gst_structure_get_uint(boxed, "width", &width)
                    && gst_structure_get_uint(boxed, "height", &height)) {
                    gst_structure_set(boxed,
                                      "x", G_TYPE_UINT, x * branch->mp_branch.input_width / branch->scale_w,
                                      "y", G_TYPE_UINT, y * branch->mp_branch.input_height / branch->scale_h,
                                      "width", G_TYPE_UINT, width * branch->mp_branch.input_width / branch->scale_w,
                                      "height", G_TYPE_UINT, height * branch->mp_branch.input_height /
                                      branch->scale_h, NULL);
                }
            }
            //let subscriber process the message
            l = msg_pro_list;
            message_ctx *t_ctx;
            while (l != NULL) {
                t_ctx  = (message_ctx *) l->data;
                t_ctx->fun(t_ctx->message_name, t_ctx->subscriber_name, message);
                l = l->next;
            }
        }
    }
    return TRUE;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis config cvsdk branch
 *
 * @Param branch
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static gboolean
cvsdk_branch_config(cvsdk_branch_t *branch)
{
    gchar desc[MAX_BUF_SIZE];
    snprintf(desc, MAX_BUF_SIZE,
             "video/x-raw,format=NV12,width=%u,height=%u,framerate=30/1",
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

    if (branch->branch_type == CVSDK_FD_BRANCH_TYPE
        && cvsdk_face_detection_heterogeneity_config_file) {
        GstElement *fd = gst_bin_get_by_name(GST_BIN(branch->mp_branch.pipeline),
                                             "fdetector");

        if (fd) {
            g_object_set(fd, "heterogeneity", DEFAULT_FD_HETEROGENEITY,
                         "heterogeneity-config-file", cvsdk_face_detection_heterogeneity_config_file,
                         NULL);
            gst_object_unref(fd);
        }
    }

    GstBus *bus = gst_element_get_bus(branch->mp_branch.pipeline);
    branch->mp_branch.bus_watch_id = gst_bus_add_watch(bus,
                                     cvsdk_branch_bus_callback, branch);
    gst_caps_unref(caps);
    gst_object_unref(bus);
    return TRUE;
}



/* --------------------------------------------------------------------------*/
/**
 * @Synopsis cvsdk custom branch init
 *
 * @Param mp_branch
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static gboolean
branch_init(mediapipe_branch_t *mp_branch)
{
    g_assert(mp_branch!=NULL);
    const gchar *desc_format = NULL;
    gchar description[MAX_BUF_SIZE];
    cvsdk_branch_t *branch = (cvsdk_branch_t *) mp_branch;
    snprintf(description, MAX_BUF_SIZE, branch->format, branch->scale_w,
             branch->scale_h);
    printf("description:[%s]\n",description);
    GstElement *new_pipeline = mediapipe_branch_create_pipeline(description);

    if (new_pipeline == NULL) {
        LOG_ERROR("Failed to create cvsdk branch, make sure you have installed cvsk plugins and configured all necessary dependencies");
        return FALSE;
    }

    mp_branch->pipeline = new_pipeline;

    if (!cvsdk_branch_config(branch)) {
        mediapipe_branch_destroy_internal(mp_branch);
        return FALSE;
    }

    return TRUE;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis parse cvsdk info from root json object and set up
 *
 * @Param mp
 * @Param root
 */
/* ----------------------------------------------------------------------------*/
static void
json_setup_cvsdk_branch(mediapipe_t *mp, struct json_object *root)
{
    struct json_object *object;
    gboolean load_success = FALSE;
    int i=0;
    RETURN_IF_FAIL(mp != NULL);
    RETURN_IF_FAIL(mp->state != STATE_NOT_CREATE);
    RETURN_IF_FAIL(json_object_object_get_ex(root, "cvsdk_detection", &object));
    ctx.cv_branch[CVSDK_MD_BRANCH_TYPE].branch_enable
        = json_check_enable_state(object, "enable_motion_detection");
    ctx.cv_branch[CVSDK_FD_BRANCH_TYPE].branch_enable
        = json_check_enable_state(object, "enable_face_detection");
    ctx.cv_branch[ICV_PD_BRANCH_TYPE].branch_enable
        = json_check_enable_state(object, "enable_people_detection");

    for (i = 0; i<BRANCH_TYPE_MAX_NUM; i++) {
        if (ctx.cv_branch[i].branch_enable) {
            if (i==CVSDK_FD_BRANCH_TYPE) {
                json_get_string(object, "fd_heterogeneity_config_file",
                                &cvsdk_face_detection_heterogeneity_config_file);
            }

            load_success = mediapipe_setup_new_branch(mp, "src", "src",
                           &ctx.cv_branch[i].mp_branch);

            if (load_success) {
                mediapipe_set_user_callback(mp, "src", "src", push_buffer_to_cvsdk_branch,
                                            &ctx.cv_branch[i]);
            }
        }
    }
}

static void
exit_master(void)
{
    gint i;

    for (i = 0; i<BRANCH_TYPE_MAX_NUM; i++) {
        if (ctx.cv_branch[i].branch_enable) {
            mediapipe_branch_destroy_internal(&ctx.cv_branch[i].mp_branch);
        }

        g_queue_free_full(ctx.cv_branch[i].smt_queue,
                          (GDestroyNotify) gst_message_unref);
        g_mutex_clear(&ctx.cv_branch[i].lock);
    }
    GList *l = g_hash_table_get_values(ctx.msg_hst);
    GList *p = l;
    while (p != NULL) {
        g_list_free_full(p->data, (GDestroyNotify)g_free);
        p = p->next;
    }
    g_list_free(l);
    g_hash_table_unref(ctx.msg_hst);
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis push buffer to cvsdk branch
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
push_buffer_to_cvsdk_branch(mediapipe_t *mp, GstBuffer *buffer, guint8 *data,
                            gsize size, gpointer user_data)
{
    cvsdk_branch_t *branch = (cvsdk_branch_t *) user_data;
    mediapipe_branch_push_buffer(&branch->mp_branch, buffer);
}

static mp_int_t
init_callback(mediapipe_t *mp)
{
    return MP_OK;
}

static mp_int_t
subscribe_message(const char *message_name,
                  const char *subscriber_name, message_process_fun fun)
{
    if (NULL == ctx.msg_hst) {
        ctx.msg_hst = g_hash_table_new_full(g_str_hash, g_str_equal,
                                            (GDestroyNotify)g_free, NULL);
        g_hash_table_insert(ctx.msg_hst, g_strdup("faces"), NULL);
        g_hash_table_insert(ctx.msg_hst, g_strdup("motions"), NULL);
        g_hash_table_insert(ctx.msg_hst, g_strdup("icvs"), NULL);
    }
    GList *msg_list = (GList *) g_hash_table_lookup(ctx.msg_hst, message_name);
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
        g_hash_table_replace(ctx.msg_hst, g_strdup(message_name), (gpointer)msg_list);
    }
}

static mp_int_t
unsubscribe_message(const char *message_name,
                    const char *subscriber_name, message_process_fun fun)
{
    if (NULL == ctx.msg_hst) {
        return MP_ERROR;
    }
    GList *msg_list = (GList *) g_hash_table_lookup(ctx.msg_hst, message_name);
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
        g_hash_table_replace(ctx.msg_hst, g_strdup(message_name), (gpointer)msg_list);
    }
    return MP_OK;
}

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
            subscribe_message(msg_name_s, subscriber_name, (message_process_fun) func);
        }
        return MP_OK;
    } else if (0 == g_strcmp0(name, "unsubscribe_message")) {
        if (gst_structure_get(s,
                              "message_name", G_TYPE_STRING, &msg_name_s,
                              "subscriber_name", G_TYPE_STRING, &subscriber_name,
                              "message_process_fun", G_TYPE_POINTER, &func,
                              NULL)) {
            unsubscribe_message(msg_name_s, subscriber_name, (message_process_fun) func);
        }
        return MP_OK;
    }
    return MP_IGNORE;
}
