/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */
/**
 * @file mp_crop_va.c
 * @Synopsis  crop gvainference or gvaclassify image accroding to the detected rectangle region
 *            and encode them to file in jpeg format
 */

#include "mediapipe_com.h"
#include <string>
#include <algorithm>
#include <vector>
#include <gst/app/app.h>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"

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

static bool IsIVAmeta_t(GstMeta *meta, int data_type)
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

static GstMetaIVA *FindIVAmeta_t(GstBuffer *buffer, int data_type,
                                 const char *model_name, const char *output_layer)
{
    GstMeta *gst_meta;
    gpointer state = NULL;
    while (gst_meta = gst_buffer_iterate_meta(buffer, &state)) {
        if (IsIVAmeta_t(gst_meta, data_type)) {
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

//new version openvo gstreamer-plugin version meta
// need crop_va sdk l_crop_va_toolkit_p_2018.4.420
// openvion gstreamer-plugins commit db7fb8a5551415a29efd490afd4ac7a190471806
#define GVA_DETECTION_META_TAG "gvadetectionmeta"
typedef struct _GVADetection GVADetection;
struct _GVADetection {
    gfloat x_min;
    gfloat y_min;
    gfloat x_max;
    gfloat y_max;
    gdouble confidence;
    gint label_id;
    gint object_id;
    GPtrArray *text_attributes;
};

typedef struct _GstGVADetectionmeta_t GstGVADetectionmeta_t;
typedef GArray BBoxesArray;
struct _GstGVADetectionmeta_t {
    GstMeta  meta;
    GstMeta *raw;
    GPtrArray *labels; // array of labels in string format with label_id as index
    BBoxesArray *bboxes;
};

static bool is_gva_detection_meta(GstMeta *meta)
{
    static GQuark gva_quark = 0;
    if (!gva_quark) {
        gva_quark = g_quark_from_static_string(GVA_DETECTION_META_TAG);
    }
    if (gst_meta_api_type_has_tag(meta->info->api, gva_quark)) {
        return true;
    }
    return false;
}
//new meta define end

#define MAX_BUF_SIZE 10240
#define QUEUE_CAPACITY 10



typedef struct _header_t {
    guint8 magic;
    guint8 version;
    guint16 meta_size;     // meta_t struct size (bytes)
    guint32 package_size;  // VideoPacket or result_packet_t size (bytes)
} header_t;

typedef struct _meta_t {
    guint8 version;
    guint8 packet_type;    // 0 (send) / 1 (receive)
    guint8 stream_id;
    guint8 num_rois;
    guint32 frame_number;
} meta_t;

typedef struct _classification_result_t {
    guint8 reserved;
    guint8 object_index;
    guint16 classification_index; // classified id from GVAClassify
    guint32 starting_offset;      // jpeg starting offset based on packet header.
    // | header_t    | meta_t     | Jpeg0   | Jpeg1   |...
    // |<- base line of offset.
    guint32 jpeg_size;                  // jpeg binary size (bytes)
    guint32 reserved2;
} classification_result_t;

typedef struct _result_packet_t {
    header_t header;
    meta_t meta;
    classification_result_t *results;
    guint8 *jpegs;
} result_packet_t;

typedef struct {
    GstElement *pipeline;
    GQueue *queue;
    guint width;    // origin image resolution width
    guint height;   // origin image resolution height
    guint sourceid;
    gboolean can_pushed; //flag for pushedable
    void *other_data; //maybe same other data need to be passed to savefile callback function
    guint roi_index; // the index of rectangle region in a buffer

    result_packet_t Jpeg_pag;
    gsize jpegmem_size;
    GSource *idle_source;
} jpeg_branch_ctx_t;

typedef struct {
    jpeg_branch_ctx_t *branch_ctx;
    guint detect_callback_id;
    GstPad *detect_pad;
    GMainContext *loop_context;
    GstElement *xlink_pipeline;
} crop_va_ctx_t;

static GstPadProbeReturn
detect_src_callback_for_crop_jpeg(GstPad *pad, GstPadProbeInfo *info,
                                  gpointer user_data);

static gboolean
push_data(gpointer user_data);

static GstPadProbeReturn
save_as_file(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);

static GstPadProbeReturn
Get_objectData(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);

static void
start_feed(GstElement *appsrc, guint unused_size, gpointer user_data);

static void
stop_feed(GstElement *appsrc, guint unused_size, gpointer user_data);

//module define start
static void
exit_master(void);

static mp_command_t  mp_crop_va_commands[] = {
    mp_null_command
};

static mp_int_t
init_callback(mediapipe_t *mp);

static void *create_ctx(mediapipe_t *mp);
static void destroy_ctx(void *_ctx);
static mp_int_t init_module(mediapipe_t *mp);

static mp_module_ctx_t mp_crop_va_module_ctx = {
    mp_string("crop_va"),
    create_ctx,
    NULL,
    destroy_ctx
};

mp_module_t  mp_crop_va_module = {
    MP_MODULE_V1,
    &mp_crop_va_module_ctx,              /* module context */
    mp_crop_va_commands,                 /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                              /* init master */
    init_module,                              /* init module */
    NULL,                              /* keyshot_process*/
    NULL,                              /* message_process */
    init_callback,                              /* init_callback */
    NULL,                              /* netcommand_process */
    exit_master,                       /* exit master */
    MP_MODULE_V1_PADDING
};


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis when quit
 */
/* ----------------------------------------------------------------------------*/
static void
exit_master(void)
{
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis get buffer from the queue, crop the image accroding to detected
 *           rectangle region and push the cropped buffer to appsrc
 *
 * @Param user_data
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/

typedef struct _params_data {
    uint8_t magic;
    uint8_t version;
    uint16_t meta_size;     // meta_t struct size (bytes)
    uint32_t package_size;  // VideoPacket or result_packet_t size (bytes)

    uint8_t metaversion;
    uint8_t packet_type;    // 0 (send) / 1 (receive)
    uint8_t stream_id;
    uint8_t num_rois;
    uint32_t frame_number;

    uint8_t reserved;
    uint8_t object_index;
    uint16_t classification_index; // expected GT value for algo validation
} param_data_t;


static gboolean
crop_and_push_buffer(GstBuffer *buffer, jpeg_branch_ctx_t *branch_ctx,
                     GstVideoRegionOfInterestMeta *meta)
{
    GstMapInfo map;
    int xmin, ymin, xmax,  ymax;
    guint size = 0;
    gchar *raw_data = NULL;
    GstBuffer *new_buf = NULL;
    GstElement  *src = NULL;
    GstCaps *caps = NULL;
    GstElement *pipeline = branch_ctx->pipeline;
    gchar caps_string[MAX_BUF_SIZE];
    GstFlowReturn ret;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        cv::Mat frame_mat(branch_ctx->height, branch_ctx->width, CV_8UC(4),
                          map.data);               //RGBA \RGBX channels 4
        //get rectangle pointer(left top and right bottom)

        xmin = (int)(meta->x);
        ymin = (int)(meta->y);
        xmax = (int)(meta->x + meta->w);
        ymax = (int)(meta->y + meta->h);
        if (xmin < 0) {
            xmin = 0;
        }
        if (ymin < 0) {
            ymin = 0;
        }
        if (xmax > branch_ctx->width) {
            xmax = branch_ctx->width;
        }
        if (ymax > branch_ctx->height) {
            ymax = branch_ctx->height;
        }
        //corp the image and copy into a new buffer
        cv::Mat croped_ref(frame_mat, cv::Rect(xmin, ymin, xmax - xmin, ymax - ymin));
        size =  croped_ref.total() * croped_ref.elemSize();
        raw_data = g_new0(char, size);
        cv::Mat crop_mat(ymax - ymin, xmax - xmin, CV_8UC(4),
                         raw_data);               //RGBA \RGBX channels 4
        croped_ref.copyTo(crop_mat);
        new_buf = gst_buffer_new_wrapped(raw_data, size);
        gst_buffer_unmap(buffer, &map);
        //reset src caps accroding to the cropped image resolution
        snprintf(caps_string, MAX_BUF_SIZE,
                 "video/x-raw,format=BGRx,width=%u,height=%u,framerate=30/1",
                 xmax - xmin, ymax - ymin);
        g_print("caps = [%s]\n", caps_string);
        src = gst_bin_get_by_name(GST_BIN(pipeline), "mysrc");
        caps = gst_caps_from_string(caps_string);
        gst_element_set_state(src, GST_STATE_NULL);
        g_object_set(src, "caps", caps, NULL);
        gst_element_set_state(src, GST_STATE_PLAYING);
        gst_caps_unref(caps);
        branch_ctx->can_pushed = FALSE;
        //push buffer to appsrc
        g_signal_emit_by_name(GST_APP_SRC(src), "push-buffer", new_buf, &ret);
        if (ret != GST_FLOW_OK) {
            LOG_ERROR(" push buffer error\n");
        }
        if (!src) {
            gst_object_unref(src);
        }
    }
}

#define PARSE_STRUCTURE(dest, key) \
    { \
        guint value; \
        if (!gst_structure_get_uint(structure, key, &value)) { \
            continue; \
        } \
        dest = value;\
    }

static gboolean
push_data(gpointer user_data)
{
    crop_va_ctx_t *ctx = (crop_va_ctx_t *)user_data;
    jpeg_branch_ctx_t *branch_ctx = ctx->branch_ctx;
    GstBuffer *buffer = NULL;
    GstMeta *gst_meta = NULL;
    gpointer state = NULL;
    GstVideoRegionOfInterestMeta *meta = NULL;
    GList *l = NULL;
    GstStructure *structure = NULL;
    const GValue *struct_tmp = NULL;
    param_data_t *params = NULL;
    if (g_queue_is_empty(branch_ctx->queue)) {
        return TRUE;
    }
    if (!branch_ctx->can_pushed) {
        return TRUE;
    }
    buffer = (GstBuffer *) g_queue_peek_head(branch_ctx->queue);
    while ((gst_meta = gst_buffer_iterate_meta(buffer, &state)) != NULL) {
        if (gst_meta->info->api != GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE) {
            continue ;
        }
        meta = (GstVideoRegionOfInterestMeta *)gst_meta;
        structure = gst_video_region_of_interest_meta_get_param(meta, "detection");

        PARSE_STRUCTURE(branch_ctx->Jpeg_pag.header.magic, "magic");
        PARSE_STRUCTURE(branch_ctx->Jpeg_pag.header.version, "version");

        PARSE_STRUCTURE(branch_ctx->Jpeg_pag.meta.version, "metaversion");
        PARSE_STRUCTURE(branch_ctx->Jpeg_pag.meta.stream_id, "stream_id");
        PARSE_STRUCTURE(branch_ctx->Jpeg_pag.meta.frame_number, "frame_number");
        PARSE_STRUCTURE(branch_ctx->Jpeg_pag.meta.num_rois, "num_rois");

        PARSE_STRUCTURE(branch_ctx->Jpeg_pag.results[branch_ctx->roi_index].classification_index, "classification_index");

        branch_ctx->Jpeg_pag.results[branch_ctx->roi_index].object_index = branch_ctx->roi_index + 1;
        branch_ctx->Jpeg_pag.header.meta_size = sizeof(classification_result_t) * branch_ctx->Jpeg_pag.meta.num_rois;
        if (branch_ctx->roi_index < branch_ctx->Jpeg_pag.meta.num_rois) {
            LOG_DEBUG("roi_index:%d", branch_ctx->roi_index);
            crop_and_push_buffer(buffer, branch_ctx, meta);
        } else {
            //the roi info on a buffer is all processed, so pop and release
            //the buffer
            /* branch_ctx->find_package */
            branch_ctx->roi_index = 0;
            branch_ctx->jpegmem_size = 0;
            g_queue_pop_head(branch_ctx->queue);
            gst_buffer_unref(buffer);
        }
        break;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis  start to feed buffer to appsrc
 *
 * @Param appsrc
 * @Param unused_size
 * @Param user_data
 */
/* ----------------------------------------------------------------------------*/
static void
start_feed(GstElement *appsrc, guint unused_size, gpointer user_data)
{
    crop_va_ctx_t *ctx = (crop_va_ctx_t *)user_data;
    if (ctx->branch_ctx->sourceid == 0) {
#ifdef MANAGER_THREAD
        ctx->branch_ctx->source = g_idle_source_new();
        g_source_set_callback(ctx->branch->source, (GSourceFunc) push_data, ctx, NULL);
        ctx->branch_ctx->sourceid = g_source_attach(ctx->loop_context,
                                    ctx->branch_ctx->source);
#else
        ctx->branch_ctx->sourceid = g_idle_add((GSourceFunc) push_data, ctx);
#endif
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis stop to feed buffer to appsrc
 *
 * @Param appsrc
 * @Param unused_size
 * @Param user_data
 */
/* ----------------------------------------------------------------------------*/
static void
stop_feed(GstElement *appsrc, guint unused_size, gpointer user_data)
{
    crop_va_ctx_t *ctx = (crop_va_ctx_t *)user_data;
    if (ctx->branch_ctx->sourceid != 0) {
#ifdef MANAGER_THREAD
        g_source_destroy(ctx->branch_ctx->source);
#else
        g_source_remove(ctx->branch_ctx->sourceid);
#endif
        ctx->branch_ctx->sourceid = 0;
    }
}

static GstPadProbeReturn
Get_objectData(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    GstMapInfo map;
    char *pbuffer = NULL;
    char *pbegin  = NULL;
    GstBuffer *bufferTemp = NULL;
    GstElement *xlink_appsrc  = NULL;
    GstFlowReturn ret;
    crop_va_ctx_t *ctx = (crop_va_ctx_t *)user_data;
    jpeg_branch_ctx_t *branch_ctx = ctx->branch_ctx;
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        return GST_PAD_PROBE_OK;
    }
    branch_ctx->Jpeg_pag.results[branch_ctx->roi_index].jpeg_size =  map.size;
    branch_ctx->Jpeg_pag.results[branch_ctx->roi_index].starting_offset =
        branch_ctx->Jpeg_pag.header.meta_size
        + branch_ctx->jpegmem_size;//byte
    memcpy(branch_ctx->Jpeg_pag.jpegs + branch_ctx->jpegmem_size, map.data,
           map.size);
    branch_ctx->jpegmem_size += map.size;
    if (branch_ctx->roi_index + 1 == branch_ctx->Jpeg_pag.meta.num_rois) {
        branch_ctx->Jpeg_pag.header.package_size = sizeof(meta_t) +  sizeof(
                    classification_result_t) * branch_ctx->Jpeg_pag.meta.num_rois +
                branch_ctx->jpegmem_size +
                sizeof(header_t);
        branch_ctx->Jpeg_pag.header.meta_size = sizeof(meta_t) +  sizeof(
                classification_result_t);
        LOG_DEBUG("meta size:%u\n", branch_ctx->Jpeg_pag.header.meta_size);
        LOG_DEBUG("package size:%u\n", branch_ctx->Jpeg_pag.header.package_size);
        LOG_DEBUG("num_rois:%d\n", branch_ctx->Jpeg_pag.meta.num_rois);
        pbuffer = g_new0(char, branch_ctx->Jpeg_pag.header.package_size);
        pbegin = pbuffer;
        //copy
        memcpy(pbuffer, &branch_ctx->Jpeg_pag.header, sizeof(header_t));
        pbuffer += sizeof(header_t);
        memcpy(pbuffer, &branch_ctx->Jpeg_pag.meta, sizeof(meta_t));
        pbuffer += sizeof(meta_t);
        memcpy(pbuffer, branch_ctx->Jpeg_pag.results,
               sizeof(classification_result_t) * branch_ctx->Jpeg_pag.meta.num_rois);
        pbuffer += (sizeof(classification_result_t) *
                    branch_ctx->Jpeg_pag.meta.num_rois);
        memcpy(pbuffer, branch_ctx->Jpeg_pag.jpegs, branch_ctx->jpegmem_size);
        bufferTemp = gst_buffer_new_wrapped(pbegin,
                                            branch_ctx->Jpeg_pag.header.package_size);
        xlink_appsrc = (GstElement *)branch_ctx->other_data;
        g_assert(xlink_appsrc != NULL);
        g_signal_emit_by_name(xlink_appsrc, "push-buffer", bufferTemp, &ret);
        if (ret != GST_FLOW_OK) {
            LOG_ERROR(" push buffer error\n");
        }
    }
    gst_buffer_unmap(buffer, &map);
    branch_ctx->can_pushed = TRUE;
    branch_ctx->roi_index++;
    return GST_PAD_PROBE_OK;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis init the encode branch and store the buffer that will be feed to appsrc
 *           later
 *
 * @Param pad
 * @Param info
 * @Param user_data
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static GstPadProbeReturn
detect_src_callback_for_crop_jpeg(GstPad *pad, GstPadProbeInfo *info,
                                  gpointer user_data)
{
    crop_va_ctx_t *ctx = (crop_va_ctx_t *)user_data;
    GstVideoInfo video_info;
    //set init caps for appsrc
    GstCaps *caps = NULL;
    caps = gst_pad_get_current_caps(pad);
    if (!gst_video_info_from_caps(&video_info, caps)) {
        LOG_ERROR("get video info form detect caps falied");
        gst_caps_unref(caps);
        return GST_PAD_PROBE_REMOVE;
    }
    gst_caps_unref(caps);
    ctx->branch_ctx->width = video_info.width;
    ctx->branch_ctx->height = video_info.height;
    //pusb buffer to queue and process them later
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    g_queue_push_tail(ctx->branch_ctx->queue, gst_buffer_ref(buffer));
    return GST_PAD_PROBE_OK;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis add callback on detect
 *
 * @Param mp
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static mp_int_t
init_callback(mediapipe_t *mp)
{
    GstElement *detect = NULL;
    GstPad *pad = NULL;
    //add callback to get detect buffer and meta
    //
    crop_va_ctx_t *ctx = (crop_va_ctx_t *)mp_modules_find_moudle_ctx(mp, "crop_va");
    detect = gst_bin_get_by_name(GST_BIN(mp->pipeline), "detect");
    if (detect == NULL) {
        LOG_ERROR("can't find element detect, can't init callback");
        return MP_IGNORE;
    }
    pad = gst_element_get_static_pad(detect, "src");
    if (pad) {
        ctx->detect_callback_id = gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                                  detect_src_callback_for_crop_jpeg, ctx,
                                  NULL);
        ctx->detect_pad = pad;
        gst_object_unref(pad);
    }
    gst_object_unref(detect);
    return MP_OK;
}

static void *create_ctx(mediapipe_t *mp)
{
    crop_va_ctx_t *ctx = g_new0(crop_va_ctx_t, 1);
#ifdef MANAGER_THREAD
    ctx->loop_context = g_main_context_get_thread_default();
#endif
    return ctx;
}

static void destroy_ctx(void *_ctx)
{
    GQueue *queue = NULL;
    crop_va_ctx_t *ctx = (crop_va_ctx_t *)_ctx;
    jpeg_branch_ctx_t *branch_ctx = ctx->branch_ctx;
    if (branch_ctx) {
        queue =  branch_ctx->queue;
        if (queue) {
            g_queue_free_full(queue, (GDestroyNotify) gst_buffer_unref);
        }
        if (branch_ctx->sourceid != 0) {
            g_source_remove(branch_ctx->sourceid);
            branch_ctx->sourceid = 0;
        }
        if (branch_ctx->pipeline) {
            gst_element_set_state(branch_ctx->pipeline, GST_STATE_NULL);
            gst_object_unref(branch_ctx->pipeline);
        }
        g_free(branch_ctx->Jpeg_pag.results);
        g_free(branch_ctx->Jpeg_pag.jpegs);
        g_free(branch_ctx);
    }
    if (ctx->detect_callback_id != 0 && !ctx->detect_pad) {
        gst_pad_remove_probe(ctx->detect_pad, ctx->detect_callback_id);
    }
    if (ctx->xlink_pipeline) {
        gst_element_set_state(ctx->xlink_pipeline, GST_STATE_NULL);
        gst_object_unref(ctx->xlink_pipeline);
    }
    g_free(ctx);
}

static mp_int_t
init_module(mediapipe_t *mp)
{
    GstElement *xlink_appsrc = NULL;
    GstElement *jpeg_appsrc = NULL;
    GstElement  *jpeg_enc = NULL;
    GstCaps *caps = NULL;
    GstVideoInfo video_info;
    gchar caps_string[MAX_BUF_SIZE];
    GstPad *jpeg_src_pad = NULL;
    GstStateChangeReturn ret =  GST_STATE_CHANGE_FAILURE;
    crop_va_ctx_t *ctx = (crop_va_ctx_t *)mp_modules_find_moudle_ctx(mp, "crop_va");
    ctx->branch_ctx = g_new0(jpeg_branch_ctx_t, 1);
    ctx->branch_ctx->queue = g_queue_new();
    ctx->branch_ctx->pipeline =
        mediapipe_branch_create_pipeline("appsrc name=mysrc ! videoconvert ! jpegenc  name=enc ! fakesink name=mysink");
    /* mediapipe_branch_create_pipeline("appsrc name=mysrc ! videoconvert ! video/x-raw,format=NV12 ! vaapijpegenc name=enc ! fakesink name=mysink "); */
    if (ctx->branch_ctx->pipeline == NULL) {
        LOG_ERROR("create jpeg encode pipeline failed");
        return  MP_ERROR;
    }
    ctx->xlink_pipeline =
        mediapipe_branch_create_pipeline("appsrc name=myxlinksrc ! xlinksink");
    if (ctx->xlink_pipeline == NULL) {
        LOG_ERROR("create pipeline1 pipeline failed");
        return  MP_ERROR;
    }
    //get the useful element  from pipeline
    xlink_appsrc = gst_bin_get_by_name(GST_BIN(ctx->xlink_pipeline), "myxlinksrc");
    if (xlink_appsrc == NULL) {
        LOG_ERROR("get xlink appsrc failed");
        return  MP_ERROR;
    }
    g_object_set(G_OBJECT(xlink_appsrc),
                 "stream-type", 0,
                 "format", GST_FORMAT_TIME, NULL);
    ctx->branch_ctx->other_data = xlink_appsrc;
    ctx->branch_ctx->can_pushed = TRUE;
    ctx->branch_ctx->roi_index = 0;
    ctx->branch_ctx->jpegmem_size = 0;
    ctx->branch_ctx->Jpeg_pag.results = g_new0(classification_result_t, 10);
    ctx->branch_ctx->Jpeg_pag.jpegs = g_new0(guint8, MAX_BUF_SIZE * 1000);

    jpeg_appsrc = gst_bin_get_by_name(GST_BIN(ctx->branch_ctx->pipeline), "mysrc");
    if (!jpeg_appsrc) {
        LOG_ERROR("can't find jpegpipline appsrc");
        return MP_ERROR;
    }

    g_signal_connect(jpeg_appsrc, "need-data", G_CALLBACK(start_feed), ctx);
    g_signal_connect(jpeg_appsrc, "enough-data", G_CALLBACK(stop_feed), ctx);
    gst_object_unref(jpeg_appsrc);
    //add callback on enc to save jpeg data
    jpeg_enc = gst_bin_get_by_name(GST_BIN(ctx->branch_ctx->pipeline), "enc");
    if (jpeg_enc == NULL) {
        LOG_ERROR("can't find element jpeg encode");
        return MP_ERROR;
    }
    jpeg_src_pad = gst_element_get_static_pad(jpeg_enc, "src");
    if (jpeg_src_pad == NULL) {
        LOG_ERROR("can't get jpeg encode src pad");
        return MP_ERROR;
    }
    gst_pad_add_probe(jpeg_src_pad, GST_PAD_PROBE_TYPE_BUFFER,
                      Get_objectData, ctx, NULL);
    gst_object_unref(jpeg_src_pad);
    gst_object_unref(jpeg_enc);
    ret = gst_element_set_state(ctx->branch_ctx->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return MP_ERROR;
    }
    ret = gst_element_set_state(ctx->xlink_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return MP_ERROR;
    }
    return MP_OK;
}
