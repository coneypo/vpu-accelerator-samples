/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */
/**
 * @file mp_gva_postproc_and_upload.c
 * @Synopsis  crop gvainference or gvaclassify image accroding to the detected rectangle region
 *            and encode them to file in jpeg format
 */

#include "mediapipe_com.h"
#include "utils/packet_struct_v3.h"
#include <string>
#include <algorithm>
#include <vector>
#include <gst/app/app.h>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"

#define MAX_BUF_SIZE 1024
#define QUEUE_CAPACITY 10

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
    GstElement *pipeline;
    GstPad *enc_pad;
    guint enc_callback_id;
    GQueue *queue;
    GAsyncQueue *jpeg_roi_queue;
    guint width;    // origin image resolution width
    guint height;   // origin image resolution height
    guint sourceid;
    gboolean can_pushed; //flag for pushedable
    void *other_data; //maybe same other data need to be passed to savefile callback function
    guint roi_index; // the index of rectangle region in a buffer
    ResultPacket Jpeg_pag; // the binary packet sent back to IA
    gsize jpegmem_size; //the total jpeg size of current picture
    guint malloc_max_roi_size;
    guint malloc_max_jpeg_size;
    gint format;
    gint roi_threshold;
} jpeg_branch_ctx_t;

typedef struct {
    mediapipe_t *mp;
    jpeg_branch_ctx_t *branch_ctx;
    guint detect_callback_id;
    GstPad *detect_pad;
    GstElement *xlink_pipeline;
    guint channel;
} gva_postproc_and_upload_ctx_t;

static GstPadProbeReturn
detect_src_callback_for_crop_jpeg(GstPad *pad, GstPadProbeInfo *info,
                                  gpointer user_data);

static char *
mp_parse_config(mediapipe_t *mp, mp_command_t *cmd);

static gboolean
push_data(gpointer user_data);

#if SWITCHON
static GstPadProbeReturn
save_as_file(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
#endif

static GstPadProbeReturn
Get_objectData(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);

static void
start_feed(GstElement *appsrc, guint unused_size, gpointer user_data);

static void
stop_feed(GstElement *appsrc, gpointer user_data);

static uint8_t
get_receive_flag_by_send_flag(uint8_t sendFlag);

static gboolean
send_result_back(jpeg_branch_ctx_t* branch_ctx);

static gboolean
push_none_roi_data(GstBuffer *buffer, jpeg_branch_ctx_t *branch_ctx,
                          GstVideoRegionOfInterestMeta *meta);

static gboolean
process_roi_only(jpeg_branch_ctx_t *branch_ctx, gint roi_index);

//module define start
static void
exit_master(void);

static mp_command_t  mp_gva_postproc_and_upload_commands[] = {
   {
        mp_string("gva_postproc_and_upload"),
        MP_MAIN_CONF,
        mp_parse_config,
        0,
        0,
        NULL
    },
    mp_null_command
};



static mp_int_t
init_callback(mediapipe_t *mp);

static void *create_ctx(mediapipe_t *mp);
static void destroy_ctx(void *_ctx);
static mp_int_t init_module(mediapipe_t *mp);

static mp_module_ctx_t mp_gva_postproc_and_upload_module_ctx = {
    mp_string("gva_postproc_and_upload"),
    create_ctx,
    NULL,
    destroy_ctx
};

mp_module_t  mp_gva_postproc_and_upload_module = {
    MP_MODULE_V1,
    &mp_gva_postproc_and_upload_module_ctx,              /* module context */
    mp_gva_postproc_and_upload_commands,                 /* module directives */
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
roi_rounding(int* xmin, int* ymin, int* xmax, int* ymax, guint *crop_width, guint *crop_height, guint height, guint width)
{
    if (*xmin < 0) {
        *xmin = 0;
    }
    if (*ymin < 0) {
        *ymin = 0;
    }
    if (*xmax < 0) {
        *xmax = 0;
    }
    if (*ymax < 0) {
        *ymax = 0;
    }
    if (*xmax > (int)width) {
        *xmax = width;
    }
    if (*ymax > (int)height) {
        *ymax = height;
    }
    if (*xmin > (int)width) {
        *xmin = width;
    }
    if (*ymin > (int)height) {
        *ymin = height;
    }
    if (*xmax - *xmin < 32) {
        *xmax = *xmin + 32;
        if (*xmax > (int)width) {
            *xmax = width;
            *xmin = *xmax - 32;
        }
    }
    if (*ymax - *ymin < 32) {
        *ymax = *ymin + 32;
        if (*ymax > (int)height) {
            *ymax = height;
            *ymin = *ymax - 32;
        }
    }

    /*TODO:The coordinates of the crop-roi are restricted by the vaapijpegenc plugin
        *     The following method must be used.
        *     The modified crop-roi coordinates will continue to be used in the pipeline.*/
    //nv12 must width align 16 ,height align 2
    *crop_width = ALIGN_POW2(*xmax - *xmin, 16);
    *crop_height = ALIGN_POW2(*ymax - *ymin, 2) ;
    *xmin = ALIGN_POW2(*xmin, 2);
    *ymin = ALIGN_POW2(*ymin, 2);
    if (*crop_width + *xmin > width) {
        *xmin = width - *crop_width;
    }
    if (*crop_height + *ymin > height) {
        *ymin = height - *crop_height;
    }
    if (*xmin < 0 || *ymin < 0) {
        GST_ERROR("The crop range of rectangle is error,\
                xmin:%d, ymin:%d, width:%d, height:%d",
                    (int)*xmin, (int)*ymin, (int)width, (int)height);
        return false;
    }
    return true;
}

static gboolean 
is_roi_outdated(GstVideoRegionOfInterestMeta *roi_meta, jpeg_branch_ctx_t *branch_ctx)
{
    for (GList *l = roi_meta->params; l; l = g_list_next(l)) {
        GstStructure *structure = (GstStructure *) l->data;
        int frames_ago = 0;
        if (gst_structure_has_field(structure, "frames_ago") &&
            gst_structure_get_int(structure, "frames_ago", &frames_ago)) {
            if(frames_ago > branch_ctx->roi_threshold) {
                LOG_DEBUG("MEDIAPIPE|ROIENC|DEBUG|Find too old ROIs frames_ago: %d threshold %d\n", frames_ago, branch_ctx->roi_threshold);
                return true;
            }
        }
    }
    return false;
}


static gboolean
send_result_back(jpeg_branch_ctx_t* branch_ctx)
{
    char *pbuffer = NULL;
    char *pbegin  = NULL;
    GstBuffer *result_buf = NULL;
    GstElement *xlink_appsrc  = NULL;
    GstFlowReturn ret;
    //the caller needs to ensure that the branch_ctx is correct.
    LOG_DEBUG("meta size:%u\n", branch_ctx->Jpeg_pag.header.meta_size);
    LOG_DEBUG("package size:%u\n", branch_ctx->Jpeg_pag.header.package_size);
    LOG_DEBUG("num_rois:%d\n", branch_ctx->Jpeg_pag.meta.num_rois);
    pbuffer = g_new0(char, branch_ctx->Jpeg_pag.header.package_size);
    pbegin = pbuffer;
    //copy header
    memcpy(pbuffer, &branch_ctx->Jpeg_pag.header, sizeof(Header));
    pbuffer += sizeof(Header);
    //copy meta
    memcpy(pbuffer, &branch_ctx->Jpeg_pag.meta, sizeof(Meta));
    pbuffer += sizeof(Meta);
    //copy jpeg
    if(branch_ctx->Jpeg_pag.meta.num_rois){
        //copy the classification result
        memcpy(pbuffer, branch_ctx->Jpeg_pag.results,
            sizeof(ClassificationResult) * branch_ctx->Jpeg_pag.meta.num_rois);
        pbuffer += (sizeof(ClassificationResult) * branch_ctx->Jpeg_pag.meta.num_rois);
        //only copy the jpeg when the jpegmem_size is not 0
        if(branch_ctx->jpegmem_size)
            memcpy(pbuffer, branch_ctx->Jpeg_pag.jpegs, branch_ctx->jpegmem_size);
    }
    result_buf = gst_buffer_new_wrapped(pbegin,
            branch_ctx->Jpeg_pag.header.package_size);
    xlink_appsrc = (GstElement *)branch_ctx->other_data;
    g_assert(xlink_appsrc != NULL);
    GST_WARNING("MEDIAPIPE|ROIENC|Send to HDDLSink GstBufferSize %ld\n", gst_buffer_get_size(result_buf));
    g_signal_emit_by_name(xlink_appsrc, "push-buffer", result_buf, &ret);
    gst_buffer_unref(result_buf);
    if (ret != GST_FLOW_OK) {
        LOG_ERROR(" push buffer error\n");
	return false;
    }

    return true;
}

static gboolean
process_roi_only(jpeg_branch_ctx_t *branch_ctx, gint roi_index)
{
    //Format the result packet without JPEG
    branch_ctx->Jpeg_pag.results[roi_index].jpeg_size =  0;
    branch_ctx->Jpeg_pag.results[roi_index].starting_offset = 0;
    branch_ctx->roi_index++;
    if (branch_ctx->roi_index == branch_ctx->Jpeg_pag.meta.num_rois) {
        GST_LOG("MEDIAPIPE|ROIENC|DEBUG|Send Result back via process_roi_only");
        branch_ctx->Jpeg_pag.header.package_size = sizeof(Header)
                + branch_ctx->Jpeg_pag.header.meta_size
                + sizeof(ClassificationResult) * branch_ctx->Jpeg_pag.meta.num_rois
                + branch_ctx->jpegmem_size;
        branch_ctx->Jpeg_pag.meta.packet_type = get_receive_flag_by_send_flag(branch_ctx->Jpeg_pag.meta.packet_type);
        //set packet_type
        send_result_back(branch_ctx);
    }
    return true;
}

static gboolean
crop_and_push_buffer_BGRA(GstBuffer *buffer, jpeg_branch_ctx_t *branch_ctx,
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
    gboolean bFlag = false;
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
        if (xmax < 0) {
            xmax = 0;
        }
        if (ymax < 0) {
            ymax = 0;
        }
        if (xmax > (int)(branch_ctx->width)) {
            xmax = branch_ctx->width;
        }
        if (ymax > (int)(branch_ctx->height)) {
            ymax = branch_ctx->height;
        }
        if (xmin > (int)(branch_ctx->width)) {
            xmin = branch_ctx->width;
        }
        if (ymin > (int)(branch_ctx->height)) {
            ymin = branch_ctx->height;
        }
        //only image size is bigger than 32*32, the jpgenc will encode image.
        bFlag = false;
        if (xmax - xmin < 32) {
            xmax = xmin + 32;
            if(xmax > (int)(branch_ctx->width)) {
                xmax = branch_ctx->width;
                xmin = xmax - 32;
            }
            bFlag = true;
        }
        if (ymax - ymin < 32) {
            ymax = ymin + 32;
            if(ymax > (int)(branch_ctx->height)) {
                ymax = branch_ctx->height;
                ymin = ymax - 32;
            }
            bFlag = true;
        }
        if(bFlag) {
            LOG_INFO("The range of rectangle is smaller than 32×32, has be increased !");
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
        LOG_DEBUG("caps = [%s]\n", caps_string);
        src = gst_bin_get_by_name(GST_BIN(pipeline), "mysrc");
        caps = gst_caps_from_string(caps_string);
        /* gst_element_set_state(src, GST_STATE_NULL); */
        g_object_set(src, "caps", caps, NULL);
        /* gst_element_set_state(src, GST_STATE_PLAYING); */
        gst_caps_unref(caps);
        branch_ctx->can_pushed = FALSE;
        //push buffer to appsrc
        g_signal_emit_by_name(GST_APP_SRC(src), "push-buffer", new_buf, &ret);
        gst_buffer_unref(new_buf);
        if (ret != GST_FLOW_OK) {
            LOG_ERROR(" push buffer error\n");
        }
        gst_object_unref(src);
    }
    return true;
}

static gboolean
push_none_roi_data(GstBuffer *buffer, jpeg_branch_ctx_t *branch_ctx,
                          GstVideoRegionOfInterestMeta *meta)
{
    branch_ctx->Jpeg_pag.meta.num_rois = 0;
    branch_ctx->Jpeg_pag.header.package_size = sizeof(Header)
        + branch_ctx->Jpeg_pag.header.meta_size;
    branch_ctx->Jpeg_pag.meta.packet_type = get_receive_flag_by_send_flag(branch_ctx->Jpeg_pag.meta.packet_type);

    gboolean ret = send_result_back(branch_ctx);
    branch_ctx->roi_index++;
    if(!ret)
        GST_WARNING("MEDIAPIPE|ROIENC|DEBUG|Send result back to IA error %d\n", ret);
    return ret;
}

static gboolean
crop_and_push_buffer_NV12(GstBuffer *buffer, jpeg_branch_ctx_t *branch_ctx,
                          GstVideoRegionOfInterestMeta *meta, gint roi_index)
{
    GstMapInfo map;
    int xmin, ymin, xmax,  ymax;
    guint crop_width = 0;
    guint crop_height = 0;
    GstElement  *src = NULL;
    GstCaps *caps = NULL;
    GstElement *pipeline = branch_ctx->pipeline;
    gchar caps_string[MAX_BUF_SIZE];
    gboolean bFlag = false;
    UNUSED(bFlag);
    GstFlowReturn ret;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ))
    {
        xmin = (int)(meta->x);
        ymin = (int)(meta->y);
        xmax = (int)(meta->x + meta->w);
        ymax = (int)(meta->y + meta->h);

        gboolean success = roi_rounding(&xmin, &ymin, &xmax, &ymax, &crop_width, &crop_height, branch_ctx->height, branch_ctx->width);
        UNUSED(success);
        //outdated true means too old ROI, false means new ROI
        gboolean outdated = is_roi_outdated(meta, branch_ctx);
        gst_buffer_unmap(buffer, &map);

        if(outdated){
            LOG_DEBUG("MEDIAPIPE|DEBUG|Find old ROI just fill packet roi_index %d\n", roi_index);
            process_roi_only(branch_ctx, roi_index);
        } else {
            LOG_DEBUG("MEDIAPIPE|DEBUG|Find young ROI do jpegEnc %d\n", roi_index);
            gint* proi_index = g_new0(gint, 1);
            *proi_index = roi_index;
            LOG_DEBUG("MEDIAPIPE|DEBUG|roi_index %d|branch roi_index %d\n", roi_index, branch_ctx->roi_index);
            g_async_queue_push(branch_ctx->jpeg_roi_queue, proi_index);
            //reset src caps accroding to the cropped image resolution
            snprintf(caps_string, MAX_BUF_SIZE,
                    "video/x-raw(memory:DMABuf),format=NV12,width=%u,height=%u,framerate=30/1",
                    branch_ctx->width, ALIGN_POW2(branch_ctx->height, 16));
            LOG_DEBUG("caps = [%s]\n", caps_string);
            src = gst_bin_get_by_name(GST_BIN(pipeline), "mysrc");
            caps = gst_caps_from_string(caps_string);
            /* gst_element_set_state(src, GST_STATE_NULL); */
            g_object_set(src, "caps", caps, NULL);
            /* gst_element_set_state(src, GST_STATE_PLAYING); */
            gst_caps_unref(caps);

            g_snprintf(caps_string, MAX_BUF_SIZE, "%u%c%u%c%u%c%u",  xmin, ',', ymin, ',',crop_width, ',', crop_height);
            GST_WARNING("###########crop-roi = [%s]#########\n", caps_string);
            guint set_ret;
            MEDIAPIPE_SET_PROPERTY(set_ret, branch_ctx, "enc", "crop-roi", caps_string, NULL);
            if( set_ret != 0) {
            LOG_ERROR(" vaapijpegenc set crop-roi error !!\n");
            }
            branch_ctx->can_pushed = FALSE;
            //push buffer to appsrc
            g_signal_emit_by_name(GST_APP_SRC(src), "push-buffer", buffer, &ret);
            if (ret != GST_FLOW_OK) {
                LOG_ERROR(" push buffer error\n");
            }
            gst_object_unref(src);
            }
    }
    return TRUE;
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
    gva_postproc_and_upload_ctx_t *ctx = (gva_postproc_and_upload_ctx_t *)user_data;
    jpeg_branch_ctx_t *branch_ctx = ctx->branch_ctx;
    GstBuffer *buffer = NULL;
    GstMeta *gst_meta = NULL;
    gpointer state = NULL;
    GstVideoRegionOfInterestMeta *meta = NULL;
    GstStructure *structure = NULL;
    const GValue *struct_tmp = NULL;
    UNUSED(struct_tmp);
    param_data_t *params = NULL;
    UNUSED(params);
    guint memory_size = branch_ctx->malloc_max_roi_size;
    if (g_queue_is_empty(branch_ctx->queue)) {
        GST_LOG("MEDIAPIPE|ROIENC|No frame to encode\n");
        g_usleep(30000);
        return TRUE;
    }
    if (!branch_ctx->can_pushed) {
        GST_LOG("MEDIAPIPE|ROIENC|Wait for jpeg encoder ready\n");
        g_usleep(30000 / (g_queue_get_length(branch_ctx->queue)));
        return TRUE;
    }
    buffer = (GstBuffer *) g_queue_peek_head(branch_ctx->queue);
    int roi_index = 0;
    int label_id = 0;
    int roi_num = 0;
    if (branch_ctx->roi_index == 0) {
        //count total roi nums
        while ((gst_meta = gst_buffer_iterate_meta(buffer, &state)) != NULL) {
            if (gst_meta->info->api != GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE) {
                continue;
            }
            roi_num ++;
            continue;
        }
        branch_ctx->Jpeg_pag.meta.num_rois = roi_num;
        GST_WARNING("MEDIAPIPE|ROIENC|ROI Num %d\n", roi_num);
    }

    if (branch_ctx->Jpeg_pag.meta.num_rois == 0 ||
        branch_ctx->roi_index >= branch_ctx->Jpeg_pag.meta.num_rois) {
        //the roi info on a buffer is all processed, so pop and release
        branch_ctx->roi_index = 0;
        branch_ctx->jpegmem_size = 0;
        g_queue_pop_head(branch_ctx->queue);
        gst_buffer_unref(buffer);
        return TRUE;
    }

    while ((gst_meta = gst_buffer_iterate_meta(buffer, &state)) != NULL) {
        if (gst_meta->info->api != GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE) {
            continue ;
        }
        //this roi has been processed, proceeds to next
        if (roi_index < (int)(branch_ctx->roi_index)) {
            roi_index ++;
            continue;
        }

        meta = (GstVideoRegionOfInterestMeta *)gst_meta;

        for (GList* l = meta->params; l; l = g_list_next(l)) {
            structure = (GstStructure *) l->data;
            if (gst_structure_has_field(structure, "label_id") &&
                gst_structure_get_int(structure, "label_id", &label_id)) {
                LOG_DEBUG("label_id: %d", label_id);
                break;
            }
        }

        branch_ctx->Jpeg_pag.results[branch_ctx->roi_index].classification_index = label_id;
        branch_ctx->Jpeg_pag.results[branch_ctx->roi_index].left = meta->x;
        branch_ctx->Jpeg_pag.results[branch_ctx->roi_index].top =  meta->y;
        branch_ctx->Jpeg_pag.results[branch_ctx->roi_index].width = meta->w;
        branch_ctx->Jpeg_pag.results[branch_ctx->roi_index].height = meta->h;

        structure = gst_video_region_of_interest_meta_get_param(meta, "mediapipe_probe_meta");

        PARSE_STRUCTURE(branch_ctx->Jpeg_pag.header.magic, "magic");
        PARSE_STRUCTURE(branch_ctx->Jpeg_pag.header.version, "version");

        PARSE_STRUCTURE(branch_ctx->Jpeg_pag.meta.version, "metaversion");
        PARSE_STRUCTURE(branch_ctx->Jpeg_pag.meta.stream_id, "stream_id");
        PARSE_STRUCTURE(branch_ctx->Jpeg_pag.meta.frame_number, "frame_number");
        PARSE_STRUCTURE(branch_ctx->Jpeg_pag.meta.packet_type, "packet_type");
        /* PARSE_STRUCTURE(branch_ctx->Jpeg_pag.meta.num_rois, "num_rois"); */
        while (branch_ctx->Jpeg_pag.meta.num_rois > memory_size) {
            memory_size =  memory_size * 2;
        }
        if (memory_size > branch_ctx->malloc_max_roi_size) {
            ctx->branch_ctx->Jpeg_pag.results =
                (ClassificationResult*) g_realloc(ctx->branch_ctx->Jpeg_pag.results,
                        sizeof(ClassificationResult) * memory_size);
            branch_ctx->malloc_max_roi_size = memory_size;
            LOG_DEBUG("channel:%d, roi memory increase to %d", ctx->channel,
                    branch_ctx->malloc_max_roi_size);
        }

        /* PARSE_STRUCTURE(branch_ctx->Jpeg_pag.results[branch_ctx->roi_index].object_index, "object_index"); */

        //meta size is fixed , = 8;
        branch_ctx->Jpeg_pag.header.meta_size = 8;
        if (branch_ctx->roi_index < branch_ctx->Jpeg_pag.meta.num_rois) {
            LOG_DEBUG("roi_index:%d", branch_ctx->roi_index);
            if(meta->x == 0
                    && meta->y == 0
                    && meta->w == 0
                    && meta->h  == 0
                    && branch_ctx->Jpeg_pag.meta.num_rois == 1){
                push_none_roi_data(buffer, branch_ctx, meta);
            }else if(branch_ctx->format == GST_VIDEO_FORMAT_NV12 ){
                crop_and_push_buffer_NV12(buffer, branch_ctx, meta, roi_index);
            }else if(branch_ctx->format == GST_VIDEO_FORMAT_BGRA ){
                crop_and_push_buffer_BGRA(buffer, branch_ctx, meta);
            }
            else{
                LOG_ERROR("not support crop format:%d", branch_ctx->format);
            }
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
    gva_postproc_and_upload_ctx_t *ctx = (gva_postproc_and_upload_ctx_t *)user_data;
    if (!ctx->branch_ctx->sourceid) {
        GMainContext* context = g_main_loop_get_context(ctx->mp->loop);
        GSource* source = g_idle_source_new();
        g_source_set_callback(source, (GSourceFunc) push_data, ctx, NULL);
        ctx->branch_ctx->sourceid = g_source_attach(source, context);
        g_source_unref(source);
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
stop_feed(GstElement *appsrc, gpointer user_data)
{
    gva_postproc_and_upload_ctx_t *ctx = (gva_postproc_and_upload_ctx_t *)user_data;
    if (ctx->branch_ctx->sourceid != 0) {
        GMainContext* context = g_main_loop_get_context(ctx->mp->loop);
        GSource* source = g_main_context_find_source_by_id(context, ctx->branch_ctx->sourceid);
        if (source) {
            g_source_destroy(source);
        }
        ctx->branch_ctx->sourceid = 0;
    }
}

static uint8_t get_receive_flag_by_send_flag(uint8_t sendFlag)
{
    if (sendFlag < PACKET_SEND_HEVC || sendFlag > PACKET_SEND_AVC) {
        LOG_ERROR("Error packet_type of send!\n");
        return 0;
    }
    switch (sendFlag) {
        case PACKET_SEND_HEVC:
            return PACKET_RECEIVE_HEVC;
        case PACKET_SEND_JPEG:
            return PACKET_RECEIVE_JPEG;
        case PACKET_SEND_AVC:
            return PACKET_RECEIVE_AVC;
        default:
            LOG_ERROR("Error packet_type of send!\n");
            return 0;
    }
}

static GstPadProbeReturn
Get_objectData(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    GstMapInfo map;
    gva_postproc_and_upload_ctx_t *ctx = (gva_postproc_and_upload_ctx_t *)user_data;
    jpeg_branch_ctx_t *branch_ctx = ctx->branch_ctx;
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    guint memory_size = branch_ctx->malloc_max_jpeg_size;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        return GST_PAD_PROBE_OK;
    }
    gint* proi_index = (gint*)g_async_queue_pop(branch_ctx->jpeg_roi_queue);
    if (!proi_index) {
        GST_ERROR("MEDIAPIPE|ERROR|GET ROI Index Error\n");
        abort();
    }
    if(*proi_index >= branch_ctx->Jpeg_pag.meta.num_rois){
        GST_ERROR("MEDIAPIPE|ERROR|GET ROI Index Overflow roi %d|queue roi %d\n", *proi_index, branch_ctx->Jpeg_pag.meta.num_rois);
        abort();
    } 
    branch_ctx->Jpeg_pag.results[*proi_index].jpeg_size =  map.size;
    branch_ctx->Jpeg_pag.results[*proi_index].starting_offset =
        sizeof(Header) + branch_ctx->Jpeg_pag.header.meta_size +
        sizeof(ClassificationResult) * branch_ctx->Jpeg_pag.meta.num_rois +
        branch_ctx->jpegmem_size;//byte
    g_free(proi_index);
    while (branch_ctx->jpegmem_size + map.size > memory_size) {
        memory_size =  memory_size * 2;
    }

    if (memory_size > branch_ctx->malloc_max_jpeg_size) {
        ctx->branch_ctx->Jpeg_pag.jpegs = (guint8 *) g_realloc(
                                              ctx->branch_ctx->Jpeg_pag.jpegs,
                                              sizeof(guint8) * memory_size);
        branch_ctx->malloc_max_jpeg_size =  memory_size;
        LOG_DEBUG("channel:%d, jpeg memory increase to %d", ctx->channel,
                branch_ctx->malloc_max_jpeg_size);
    }

    memcpy(branch_ctx->Jpeg_pag.jpegs + branch_ctx->jpegmem_size, map.data,
           map.size);

    branch_ctx->jpegmem_size += map.size;
    if (branch_ctx->roi_index + 1 == branch_ctx->Jpeg_pag.meta.num_rois) {
        branch_ctx->Jpeg_pag.header.package_size = sizeof(Header)
                + branch_ctx->Jpeg_pag.header.meta_size
                + sizeof(ClassificationResult) * branch_ctx->Jpeg_pag.meta.num_rois
                + branch_ctx->jpegmem_size;
        branch_ctx->Jpeg_pag.meta.packet_type = get_receive_flag_by_send_flag(branch_ctx->Jpeg_pag.meta.packet_type);
        GST_LOG("MEDIAPIPE|DEBUG|Send REsult back via Get_objectData");
        send_result_back(branch_ctx);
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
    gva_postproc_and_upload_ctx_t *ctx = (gva_postproc_and_upload_ctx_t *)user_data;
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
    ctx->branch_ctx->format = GST_VIDEO_INFO_FORMAT(&video_info);
    //pusb buffer to queue and process them later
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    g_queue_push_tail(ctx->branch_ctx->queue, gst_buffer_ref(buffer));
    if (g_queue_get_length(ctx->branch_ctx->queue) > 5) {
        LOG_DEBUG("buffer queue length:%d\n", g_queue_get_length(ctx->branch_ctx->queue));
        g_usleep(30000 * g_queue_get_length(ctx->branch_ctx->queue));
    }
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
    gva_postproc_and_upload_ctx_t *ctx = (gva_postproc_and_upload_ctx_t *) mp_modules_find_module_ctx(mp,
                                                                                                      "gva_postproc_and_upload");
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
    }
    gst_object_unref(detect);
    return MP_OK;
}

static void *create_ctx(mediapipe_t *mp)
{
    gva_postproc_and_upload_ctx_t *ctx = g_new0(gva_postproc_and_upload_ctx_t, 1);
    ctx->mp = mp;
    return ctx;
}

static void destroy_ctx(void *_ctx)
{
    GQueue *queue = NULL;
    gva_postproc_and_upload_ctx_t *ctx = (gva_postproc_and_upload_ctx_t *)_ctx;
    jpeg_branch_ctx_t *branch_ctx = ctx->branch_ctx;
    if (branch_ctx) {
        queue =  branch_ctx->queue;
        if (queue) {
            g_queue_free_full(queue, (GDestroyNotify) gst_buffer_unref);
        }
            if (ctx->branch_ctx->sourceid != 0) {
                GMainContext* context = g_main_loop_get_context(ctx->mp->loop);
                GSource* source = g_main_context_find_source_by_id(context, ctx->branch_ctx->sourceid);
                if (source) {
                    g_source_destroy(source);
                }
                ctx->branch_ctx->sourceid = 0;
        }

        if (branch_ctx->enc_pad) {
            if (branch_ctx->enc_callback_id) {
                gst_pad_remove_probe(branch_ctx->enc_pad, branch_ctx->enc_callback_id);
            }
            gst_object_unref(branch_ctx->enc_pad);
        }
        if (branch_ctx->pipeline) {
            gst_element_set_state(branch_ctx->pipeline, GST_STATE_NULL);
            gst_object_unref(branch_ctx->pipeline);
        }
        g_free(branch_ctx->Jpeg_pag.results);
        g_free(branch_ctx->Jpeg_pag.jpegs);
        g_free(branch_ctx);
    }

    if (ctx->detect_pad) {
        if (ctx->detect_callback_id) {
            gst_pad_remove_probe(ctx->detect_pad, ctx->detect_callback_id);
        }
        gst_object_unref(ctx->detect_pad);
    }


    if (ctx->xlink_pipeline) {
        //send eos to xlink_pipeline
        GstElement *xlink_src = gst_bin_get_by_name(GST_BIN(ctx->xlink_pipeline), "myxlinksrc");
        if (xlink_src) {
            GstPad *src_pad = gst_element_get_static_pad(xlink_src, "src");
            if (src_pad) {
                GstEvent *event = gst_event_new_eos();
                gst_pad_push_event(src_pad, event);
                gst_object_unref(src_pad);
            } else {
                LOG_WARNING("can't find xlink pipeline myxlinksrc src pad");
            }
            gst_object_unref(xlink_src);
        } else {
            LOG_WARNING("can't find xlink pipeline src element");
        }

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
    UNUSED(caps);
    GstVideoInfo video_info;
    UNUSED(video_info);
    gchar caps_string[MAX_BUF_SIZE];
    UNUSED(caps_string);
    GstStateChangeReturn ret =  GST_STATE_CHANGE_FAILURE;
    gva_postproc_and_upload_ctx_t *ctx = (gva_postproc_and_upload_ctx_t *) mp_modules_find_module_ctx(mp,
                                                                                                      "gva_postproc_and_upload");
    ctx->branch_ctx = g_new0(jpeg_branch_ctx_t, 1);
    ctx->branch_ctx->queue = g_queue_new();
    ctx->branch_ctx->jpeg_roi_queue = g_async_queue_new();
#ifdef CROP_NV12
    ctx->branch_ctx->pipeline =
        mediapipe_branch_create_pipeline("appsrc name=mysrc ! vaapijpegenc  name=enc  crop-roi=\"100,100,200,200\" ! fakesink name=mysink");
#else
    ctx->branch_ctx->pipeline =
        mediapipe_branch_create_pipeline("appsrc name=mysrc ! videoconvert ! jpegenc  name=enc ! fakesink name=mysink");
#endif

    if (ctx->branch_ctx->pipeline == NULL) {
        LOG_ERROR("create jpeg encode pipeline failed");
        return  MP_ERROR;
    }

    jpeg_appsrc = gst_bin_get_by_name(GST_BIN(ctx->branch_ctx->pipeline), "mysrc");
    if (!jpeg_appsrc) {
        LOG_ERROR("can't find jpegpipline appsrc");
        return MP_ERROR;
    }

    g_signal_connect(jpeg_appsrc, "need-data", G_CALLBACK(start_feed), ctx);
    g_signal_connect(jpeg_appsrc, "enough-data", G_CALLBACK(stop_feed), ctx);
    gst_object_unref(jpeg_appsrc);

    ctx->xlink_pipeline = mediapipe_branch_create_pipeline("appsrc name=myxlinksrc ! hddlsink name=sink");
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
    ctx->branch_ctx->malloc_max_roi_size = 5;
    ctx->branch_ctx->malloc_max_jpeg_size = 5 * 1024 * 1024;
    ctx->branch_ctx->Jpeg_pag.results = g_new0(ClassificationResult,
                                        ctx->branch_ctx->malloc_max_roi_size);
    ctx->branch_ctx->Jpeg_pag.jpegs = g_new0(guint8,
                                      ctx->branch_ctx->malloc_max_jpeg_size);

    //add callback on enc to save jpeg data
    jpeg_enc = gst_bin_get_by_name(GST_BIN(ctx->branch_ctx->pipeline), "enc");
    if (jpeg_enc == NULL) {
        LOG_ERROR("can't find element jpeg encode");
        return MP_ERROR;
    }
    ctx->branch_ctx->enc_pad = gst_element_get_static_pad(jpeg_enc, "src");
    if (ctx->branch_ctx->enc_pad == NULL) {
        LOG_ERROR("can't get jpeg encode src pad");
        return MP_ERROR;
    }
    ctx->branch_ctx->enc_callback_id = gst_pad_add_probe(ctx->branch_ctx->enc_pad, GST_PAD_PROBE_TYPE_BUFFER,
                      Get_objectData, ctx, NULL);
    gst_object_unref(jpeg_enc);
    ret = gst_element_set_state(ctx->branch_ctx->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return MP_ERROR;
    }
    return MP_OK;
}

static char *mp_parse_config(mediapipe_t *mp, mp_command_t *cmd)
{
    int channelId = -1;
    if (!mediapipe_get_channelId(mp, "sink", &channelId)) {
        return MP_CONF_OK;
    }

    struct json_object *parent;
    struct json_object *root = mp->config;
    gva_postproc_and_upload_ctx_t* ctx = (gva_postproc_and_upload_ctx_t*) mp_modules_find_module_ctx(mp, "gva_postproc_and_upload");
    // set the defaul roi_threshold to 30
    ctx->branch_ctx->roi_threshold = 5;
    if (json_object_object_get_ex(root, "gva_postproc_and_upload", &parent)){
        struct json_object *roi_config = NULL;
        if(json_object_object_get_ex(parent, "roi_threshold", &roi_config)) {
            gint roi_threshold = json_object_get_int(roi_config);
            GST_WARNING("MEDIAPIPE|DEBUG|set roi_threshold %d\n",roi_threshold);
            ctx->branch_ctx->roi_threshold = roi_threshold;
        }
    }


    GstElement* sink = gst_bin_get_by_name(GST_BIN(ctx->xlink_pipeline), "sink");
    if (!sink) {
        LOG_WARNING("cannot find element named \"sink\".");
        return MP_CONF_OK;
    }

    if (g_object_class_find_property(G_OBJECT_GET_CLASS(sink), "selected-target-context")) {
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
        g_object_set(sink, "selected-target-context", context, NULL);
        gst_hddl_context_free(context);
        LOG_INFO("set \"selected-target-context\" with channelId(%d) on element named \"sink\".", channelId);
    } else {
        LOG_WARNING("cannot find property \"selected-target-context\" on element named \"sink\".");
    }

    GstStateChangeReturn ret = gst_element_set_state(ctx->xlink_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOG_ERROR("failed to change xlink_pipeline state to PLAYING.");
        return (char*)MP_CONF_ERROR;
    }

    gst_object_unref(sink);
    ctx->channel = channelId;

    return MP_CONF_OK;
}
