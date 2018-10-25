#include "mediapipe_com.h"
#include <string>
#include <algorithm>

//meta define start
#define IVA_FOURCC(c1, c2, c3, c4) (((c1) & 255) + (((c2) & 255) << 8) + (((c3) & 255) << 16) + (((c4) & 255) << 24))
#define FOURCC_OUTPUT_LAYER_F32 IVA_FOURCC('O','F','3','2')
// payload contains model output layer as float array

#define FOURCC_BOUNDING_BOXES IVA_FOURCC('O','B','J',0)
typedef struct _BoundingBox {
    float xmin;
    float xmax;
    float ymin;
    float ymax;
    float confidence;
    int label_id;
    int object_id;
} BoundingBox;

#define FOURCC_OBJECT_ATTRIBUTES   IVA_FOURCC('O','A','T','T')

typedef struct {
    char as_string[256];
} ObjectAttributes;

#define IVA_META_TAG "ivameta"
#define nullptr NULL
struct GstMetaIVA {
    GstMeta meta;
    void *data;
    int data_type;
    int element_size;
    int number_elements;
    char *model_name;
    char *output_layer;
    template<typename T> T *GetElement(int index)
    {
        if (index < 0 || index > number_elements) {
            return nullptr;
        }
        if (element_size != sizeof(T)) {
            return nullptr;
        }
        return (T *)((int8_t *)data + index * element_size);
    }
    void SetModelName(const char *str)
    {
        if (model_name) {
            free((void *)model_name);
        }
        if (str) {
            model_name = (char *) malloc(strlen(str) + 1);
            strcpy(model_name, str);
        } else {
            model_name = nullptr;
        }
    }
    void SetOutputLayer(const char *str)
    {
        if (output_layer) {
            free((void *)output_layer);
        }
        if (str) {
            output_layer = (char *) malloc(strlen(str) + 1);
            strcpy(output_layer, str);
        } else {
            output_layer = nullptr;
        }
    }
};

static bool IsIVAMeta(GstMeta *meta, int data_type)
{
    static GQuark iva_quark = 0;
    if (!iva_quark) {
        iva_quark = g_quark_from_static_string(IVA_META_TAG);
    }
    if (gst_meta_api_type_has_tag(meta->info->api, iva_quark)) {
        if (data_type == 0 || ((GstMetaIVA *)meta)->data_type == data_type) {
            return true;
        }
        return false;
    }
    return false;
}

static GstMetaIVA *FindIVAMeta(GstBuffer *buffer, int data_type,
                               const char *model_name, const char *output_layer)
{
    GstMeta *gst_meta;
    gpointer state = NULL;
    while (gst_meta = gst_buffer_iterate_meta(buffer, &state)) {
        if (IsIVAMeta(gst_meta, data_type)) {
            GstMetaIVA *meta = (GstMetaIVA *) gst_meta;
            if (model_name && meta->model_name && !strstr(meta->model_name, model_name)) {
                continue;
            }
            if (output_layer && meta->output_layer
                && !strstr(meta->output_layer, output_layer)) {
                continue;
            }
            return meta;
        }
    }
    return NULL;
}
//*meta define end

//about subscriber_message start
typedef int (*message_process_fun)(const char *message_name,
                                   const char *subscribe_name, GstMessage *message);

typedef struct {
    const char *message_name;
    const char *subscriber_name;
    message_process_fun fun;
} message_ctx;

static mp_int_t
subscribe_message(const char *message_name,
                  const char *subscriber_name, message_process_fun fun);
static mp_int_t
unsubscribe_message(const char *message_name,
                    const char *subscriber_name, message_process_fun fun);

//about subscriber_message end

#define MAX_BUF_SIZE 1024
#define QUEUE_CAPACITY 10

// about branch start
typedef struct {
    mediapipe_branch_t mp_branch;
    gboolean enable;
    const gchar  *pipe_string;
    const gchar  *name;
    const gchar  *src_name;
    const gchar  *model_path1;
    const gchar  *model_path2;
    const gchar  *model_path3;
    const gchar *message_name;
    const gchar *src_format;
} openvino_branch_t;

typedef  openvino_branch_t branch_t;

typedef struct {
    mediapipe_t *mp;
    GHashTable  *msg_hst;
    guint branch_num;
    openvino_branch_t branch[1];
} openvino_ctx_t;

typedef  openvino_ctx_t ctx_t;

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

gboolean
get_value_from_buffer(GstPad *pad, GstBuffer *buffer, GValue *objectlist);

//get message and progress it end

static ctx_t  *ctx = NULL;

//module define start
static void
exit_master(void);

static  mp_int_t
message_process(mediapipe_t *mp, void *message);

static char *
mp_parse_config(mediapipe_t *mp, mp_command_t *cmd);

static mp_command_t  mp_openvino_commands[] = {
    {
        mp_string("openvino"),
        MP_MAIN_CONF,
        mp_parse_config,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_core_module_t  mp_openvino_module_ctx = {
    mp_string("openvino"),
    NULL,
    NULL
};

mp_module_t  mp_openvino_module = {
    MP_MODULE_V1,
    &mp_openvino_module_ctx,              /* module context */
    mp_openvino_commands,                 /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                              /* init master */
    NULL,                              /* init module */
    NULL,                              /* keyshot_process*/
    message_process,                              /* message_process */
    NULL,                              /* init_callback */
    NULL,                              /* netcommand_process */
    exit_master,                       /* exit master */
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
 * @Synopsis config openvino branch
 *
 * @Param branch openvino branch
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static gboolean
branch_config(branch_t *branch)
{
    gchar desc[MAX_BUF_SIZE];
    snprintf(desc, MAX_BUF_SIZE,
             "video/x-raw,format=%s,width=%u,height=%u,framerate=30/1", branch->src_format,
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
        gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, detect_src_callback, branch,
                          NULL);
        gst_object_unref(pad);
    }
    gst_object_unref(detect);
    return TRUE;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis openvino custom branch init
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
    if (branch->model_path3 != NULL) {
        snprintf(description, MAX_BUF_SIZE, branch->pipe_string, branch->model_path1,
                 branch->model_path2, branch->model_path3);
    } else if (branch->model_path2 != NULL) {
        snprintf(description, MAX_BUF_SIZE, branch->pipe_string, branch->model_path1,
                 branch->model_path2);
    } else if (branch->model_path1 != NULL) {
        snprintf(description, MAX_BUF_SIZE, branch->pipe_string, branch->model_path1);
    } else {
        snprintf(description, MAX_BUF_SIZE, branch->pipe_string, NULL);
    }
    printf("description:[%s]\n", description);
    GstElement *new_pipeline = mediapipe_branch_create_pipeline(description);
    if (new_pipeline == NULL) {
        LOG_ERROR("Failed to create openvino branch, make sure you have installed \
                openvino plugins and configured all necessary dependencies");
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
 * @Synopsis parse openvino info from root json object and set up
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
    int branch_num = json_object_array_length(object);
    ctx = (ctx_t *)g_malloc0(sizeof(ctx_t) + sizeof(
                                 branch_t) * branch_num);
    ctx->mp = mp;
    ctx->branch_num = branch_num;
    for (int i = 0; i < branch_num; ++i) {
        detect = json_object_array_get_idx(object, i);
        if (json_check_enable_state(detect, "enable")) {
            json_get_string(detect, "src_name",
                            &(ctx->branch[i].src_name));
            json_get_string(detect, "src_format",
                            &(ctx->branch[i].src_format));
            json_get_string(detect, "model_path1",
                            &(ctx->branch[i].model_path1));
            json_get_string(detect, "model_path2",
                            &(ctx->branch[i].model_path2));
            json_get_string(detect, "model_path3",
                            &(ctx->branch[i].model_path3));
            json_get_string(detect, "name",
                            &(ctx->branch[i].name));
            json_get_string(detect, "message_name",
                            &(ctx->branch[i].message_name));
            json_get_string(detect, "pipe_string",
                            &(ctx->branch[i].pipe_string));
            ctx->branch[i].enable = TRUE;
            ctx->branch[i].mp_branch.branch_init = branch_init;
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

static void
/* --------------------------------------------------------------------------*/
/**
 * @Synopsis when quit
 */
/* ----------------------------------------------------------------------------*/
exit_master(void)
{
    gint i;
    for (i = 0; i < ctx->branch_num; i++) {
        if (ctx->branch[i].enable) {
            mediapipe_branch_destroy_internal(&ctx->branch[i].mp_branch);
        }
    }
    if (ctx->msg_hst) {
        GList *l = g_hash_table_get_values(ctx->msg_hst);
        GList *p = l;
        while (p != NULL) {
            g_list_free_full((GList*)p->data, (GDestroyNotify)g_free);
            p = p->next;
        }
        g_list_free(l);
        g_hash_table_unref(ctx->msg_hst);
    }
    g_free(ctx);
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis push buffer to openvino branch
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
    mediapipe_branch_push_buffer(&branch->mp_branch, buffer);
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
static mp_int_t
subscribe_message(const char *message_name,
                  const char *subscriber_name, message_process_fun fun)
{
    if (NULL == ctx->msg_hst) {
        ctx->msg_hst = g_hash_table_new_full(g_str_hash, g_str_equal,
                                             (GDestroyNotify)g_free, NULL);
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
unsubscribe_message(const char *message_name,
                    const char *subscriber_name, message_process_fun fun)
{
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

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis  get data from buffer meta
 *
 * @Param pad
 * @Param buffer
 * @Param objectlist store the data
 */
/* ----------------------------------------------------------------------------*/
gboolean
get_value_from_buffer(GstPad *pad, GstBuffer *buffer, GValue *objectlist)
{
    GstMetaIVA *meta_vehicle_color = FindIVAMeta(buffer, FOURCC_OUTPUT_LAYER_F32,
                                     "vehicle-attributes",
                                     "color");
    GstMetaIVA *meta_vehicle_type  = FindIVAMeta(buffer, FOURCC_OUTPUT_LAYER_F32,
                                     "vehicle-attributes",
                                     "type");
    GstMetaIVA *meta_license_plate = FindIVAMeta(buffer, FOURCC_OUTPUT_LAYER_F32,
                                     "license-plate-recognition",
                                     nullptr);
    // Bounding boxes and attributes
    GstMetaIVA *meta = FindIVAMeta(buffer, FOURCC_BOUNDING_BOXES, NULL, NULL);
    if (meta != nullptr) {
        GstCaps *caps = gst_pad_get_current_caps(pad);
        GstVideoInfo info;
        if (!gst_video_info_from_caps(&info, caps)) {
            LOG_ERROR("get video info form caps falied");
            gst_caps_unref(caps);
            return FALSE;
        }
        gst_caps_unref(caps);
        GstMetaIVA *meta_attr = FindIVAMeta(buffer, FOURCC_OBJECT_ATTRIBUTES, NULL,
                                            NULL);
        if (meta->number_elements == 0) {
            return FALSE;
        }
        for (int i = 0; i < meta->number_elements; i++) {
            BoundingBox *bbox = meta->GetElement<BoundingBox>(i);
            int label_id = bbox->label_id;
            float confidence = bbox->confidence;
            float xmin = bbox->xmin * info.width;
            float ymin = bbox->ymin * info.height;
            float xmax = bbox->xmax * info.width;
            float ymax = bbox->ymax * info.height;
            const char *attributes_str = NULL;
            const char *color_str = NULL;
            const char *vehicle_str = NULL;
            const char *license_plate_str = NULL;
            if (meta_attr) {
                attributes_str = ((ObjectAttributes *)meta_attr->data)[i].as_string;
            }
            if (meta_vehicle_color) {
                static const std::string colors[] = {"white", "gray", "yellow", "red", "green", "blue", "black"};
                auto data = meta_vehicle_color->GetElement<float[7]>(i);
                if (data) {
                    float *p = *data;
                    float *p_max = std::max_element(p, p + 7);
                    if (*p_max > 0.5) {
                        color_str = colors[p_max - p].c_str();
                    }
                }
            }
            if (meta_vehicle_type) {
                static const std::string types[] = {"car", "van", "truck", "bus"};
                auto data = meta_vehicle_type->GetElement<float[4]>(i);
                if (data) {
                    float *p = *data;
                    float *p_max = std::max_element(p, p + 4);
                    if (*p_max > 0.5) {
                        vehicle_str = types[p_max - p].c_str();
                    }
                }
            }
            if (meta_license_plate) {
                static std::vector<std::string> items = {
                    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                    "<Anhui>", "<Beijing>", "<Chongqing>", "<Fujian>",
                    "<Gansu>", "<Guangdong>", "<Guangxi>", "<Guizhou>",
                    "<Hainan>", "<Hebei>", "<Heilongjiang>", "<Henan>",
                    "<HongKong>", "<Hubei>", "<Hunan>", "<InnerMongolia>",
                    "<Jiangsu>", "<Jiangxi>", "<Jilin>", "<Liaoning>",
                    "<Macau>", "<Ningxia>", "<Qinghai>", "<Shaanxi>",
                    "<Shandong>", "<Shanghai>", "<Shanxi>", "<Sichuan>",
                    "<Tianjin>", "<Tibet>", "<Xinjiang>", "<Yunnan>",
                    "<Zhejiang>", "<police>",
                    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
                    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
                    "U", "V", "W", "X", "Y", "Z"
                };
                auto data = meta_license_plate->GetElement<float[88]>(i);
                if (data) {
                    std::string license_plate;
                    int val = 0;
                    for (int i = 0; i < 88; i++) {
                        val = (*data)[i];
                        if (val < 0) {
                            break;
                        }
                        if (val < items.size()) {
                            license_plate += items[val];
                        }
                    }
                    if (val < 0) {
                        license_plate_str = license_plate.c_str();
                    }
                }
            }
            //create content list
            GValue tmp_value = { 0 };
            g_value_init(&tmp_value, GST_TYPE_STRUCTURE);
            GstStructure *s =
                gst_structure_new("object",
                                  "x", G_TYPE_UINT, int(xmin),
                                  "y", G_TYPE_UINT, int(ymin),
                                  "width", G_TYPE_UINT, int(xmax - xmin),
                                  "height", G_TYPE_UINT, int(ymax - ymin),
                                  "label_id", G_TYPE_INT, label_id,
                                  "confidence", G_TYPE_FLOAT, confidence,
                                  "attributes", G_TYPE_STRING, attributes_str,
                                  "color", G_TYPE_STRING, color_str,
                                  "vehicle", G_TYPE_STRING, vehicle_str,
                                  "license_plate", G_TYPE_STRING, license_plate_str,
                                  NULL);
            g_value_take_boxed(&tmp_value, s);
            gst_value_list_append_value(objectlist, &tmp_value);
            g_value_unset(&tmp_value);
            LOG_DEBUG("%d,%d,%d,%d,%d,%f,%s,%s,%s,%s", int(xmin), int(ymin),
                      int(xmax - xmin), int(ymax - ymin), label_id, confidence,
                      attributes_str, color_str, vehicle_str, license_plate_str);
        }
        return TRUE;
    }
    return FALSE;
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
    if (NULL == ctx->msg_hst) {
        LOG_DEBUG("%s:%d : hash table in NULL", __FILE__, __LINE__);
        return GST_PAD_PROBE_REMOVE;
    }
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    GValue objectlist = { 0 };
    g_value_init(&objectlist, GST_TYPE_LIST);
    LOG_DEBUG("branch->name:%s", branch->name);
    if (!get_value_from_buffer(pad, buffer, &objectlist)) {
        return GST_PAD_PROBE_OK;
    }
    //create messsage
    GstStructure *s = gst_structure_new(branch->name,
                                        "timestamp", G_TYPE_UINT64,
                                        GST_BUFFER_PTS(buffer),
                                        NULL);
    GstMessage *msg =  gst_message_new_element(NULL, s);
    gst_structure_set_value(
        (GstStructure *) gst_message_get_structure(msg),
        branch->message_name,
        &objectlist);
    g_value_unset(&objectlist);
    //let subscribe to progress msg
    GList *msg_pro_list = (GList *) g_hash_table_lookup(ctx->msg_hst,
                          branch->message_name);
    GList *l = NULL;
    if (NULL != msg_pro_list) {
        l = msg_pro_list;
        message_ctx *t_ctx;
        while (l != NULL) {
            t_ctx  = (message_ctx *) l->data;
            t_ctx->fun(t_ctx->message_name, t_ctx->subscriber_name, msg);
            l = l->next;
        }
    }
    gst_message_unref(msg);
    return GST_PAD_PROBE_OK;
}
