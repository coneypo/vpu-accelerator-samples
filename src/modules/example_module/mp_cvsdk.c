#include "mediapipe_com.h"
#include <gstocl/oclcommon.h>

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

typedef struct {
    cvsdk_branch_t cv_branch[BRANCH_TYPE_MAX_NUM];
    GstClockTime delay;
    mediapipe_t *mp;
} cvsdk_ctx_t;


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
    NULL,                              /* message_process */
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
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ELEMENT) {
        cvsdk_branch_t *branch = (cvsdk_branch_t *) data;
        g_queue_push_tail(branch->smt_queue, gst_message_ref(message));
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
    char *fetching_element = NULL;
    gboolean load_success = FALSE;
    int i=0;
    RETURN_IF_FAIL(mp != NULL);
    RETURN_IF_FAIL(mp->state != STATE_NOT_CREATE);
    RETURN_IF_FAIL(json_object_object_get_ex(root, "cvsdk_detection", &object));
    json_get_string(object, "fetching_element", (const char **) &fetching_element);
    RETURN_IF_FAIL(fetching_element != NULL);
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
    GList *list= NULL;
    gint i;

    for (i = 0; i<BRANCH_TYPE_MAX_NUM; i++) {
        if (ctx.cv_branch[i].branch_enable) {
            ocl_mix_meta_list_remove(element,
                                     ctx.cv_branch[i].branch_type + CVSDK_BASE_TRACKID, OCL_MIX_WIREFRAME);
            list =  query_cvsdk_udpate_branch_result(&ctx.cv_branch[i], buffer);

            if (list !=NULL) {
                ocl_mix_meta_list_append(element, list, OCL_MIX_WIREFRAME);
            }
        }
    }

    gst_object_unref(element);
    return TRUE;
}

static GList *
query_cvsdk_udpate_branch_result(cvsdk_branch_t *branch, GstBuffer *buffer)
{
    GList *ret = NULL;
    guint mp_ret = 0;
    GstClockTime msg_pts;
    GstClockTime offset = 300000000;
    const GstStructure *root, *boxed;
    guint nsize;
    GstMessage *walk;
    const GValue *vlist, *item;
    guint x,y,width,height;
    guint i;
    GstClockTime desired = GST_BUFFER_PTS(buffer);
    g_mutex_lock(&branch->lock);
    walk = (GstMessage *) g_queue_peek_head(branch->smt_queue);

    while (walk) {
        root = gst_message_get_structure(walk);
        gst_structure_get_clock_time(root, "timestamp", &msg_pts);

        if (msg_pts > desired) {
            walk = NULL;
            break;
        } else if (msg_pts == desired) {
            g_queue_pop_head(branch->smt_queue);
            break;
        } else {
            if (ctx.delay < desired - msg_pts) {
                ctx.delay = desired - msg_pts;
                MEDIAPIPE_SET_PROPERTY(mp_ret, ctx.mp, "queue_mix", "min-threshold-time",
                                       ctx.delay + offset, NULL);
                MEDIAPIPE_SET_PROPERTY(mp_ret, ctx.mp, "queue_mix", "max-size-buffers", 0,
                                       NULL);
                MEDIAPIPE_SET_PROPERTY(mp_ret, ctx.mp, "queue_mix", "max-size-time",
                                       ctx.delay + offset, NULL);
                MEDIAPIPE_SET_PROPERTY(mp_ret, ctx.mp, "queue_mix", "max-size-bytes", 0, NULL);
            }

            g_queue_pop_head(branch->smt_queue);
            gst_message_unref(walk);
            walk = (GstMessage *) g_queue_peek_head(branch->smt_queue);
        }
    }

    g_mutex_unlock(&branch->lock);

    if (walk != NULL) {
        vlist = gst_structure_get_value(root, branch->item_name);
        nsize = gst_value_list_get_size(vlist);

        for (i = 0; i < nsize; ++i) {
            item = gst_value_list_get_value(vlist, i);
            boxed =(GstStructure *) g_value_get_boxed(item);

            if (gst_structure_get_uint(boxed, "x", &x)
                && gst_structure_get_uint(boxed, "y", &y)
                && gst_structure_get_uint(boxed, "width", &width)
                && gst_structure_get_uint(boxed, "height", &height)) {
                OclMixParam *param = ocl_mix_meta_new();
                param->track_id = branch->branch_type + CVSDK_BASE_TRACKID;
                param->rect.x = (guint)(branch->mp_branch.input_width/branch->scale_w * x);
                param->rect.y = (guint)(branch->mp_branch.input_height/branch->scale_h * y);
                param->rect.width = (guint)(branch->mp_branch.input_width/branch->scale_w *
                                            width);
                param->rect.height = (guint)(branch->mp_branch.input_height/branch->scale_h *
                                             height);
                param->flag = OCL_MIX_WIREFRAME;
                ret = g_list_append(ret, param);
            }
        }

        gst_message_unref(walk);
    }

    return ret;
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
    mediapipe_set_user_callback(mp, "mix", "sink", mix_sink_callback, (void*)"mix");
    return MP_OK;
}

