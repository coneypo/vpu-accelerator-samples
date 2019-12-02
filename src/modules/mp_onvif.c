/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

/*Return client info format instruction:
  *1)SUCCESS return info format:
  *@ if the command contains "get": command@info
  *@ eg: get_image_config@{ "brightness": -79, "colorsaturation": 0, "contrast": 0, "sharpness": 63, "exposuretime": 90, "exposuremode": 0, "irismode": 0, "irislevel": 0  }\n
  *@ else: success@0
  *@ eg: success@0
  *2)ERROR return info format: error@error_number@command_length@command@info
  *@about error_number:
  *@RET_LENTH_EXCEEDING: The command is too long and the data is discarded.
  *@eg: error@-1@1620@ set_image_config@{ "brightness": -79, "colorsaturation": 0, "contrast": 0, "sharpness": 63, "exposuretime": 90, "exposuremode": 0, "irismode": 0, "irislevel": 0  }\n
  *@RET_COMMAND_ERROR: The command can not be identified.
  *@eg: error@-2@164@ set_image_config1@{ "brightness": -79, "colorsaturation": 0, "contrast": 0, "sharpness": 63, "exposuretime": 90, "exposuremode": 0, "irismode": 0, "irislevel": 0  }\n
  *@RET_PARAM_JSON_ERROR: The parameter's format of json is not right.
  *@eg: error@-3@164@ set_image_config1@{ "brightness": -79, "colorsaturation": 0, "contrast": 0, "sharpness": 63, "exposuretime": 90, "exposuremode": 0, "irismode": 0, "irislevel": 0  }\n
  *@RET_FAILED: Set failure.
  *@eg: error@-4@99@set_image_config@{ "brightness": 30, "contrast": 10, "colorsaturation": 20, "sharpness": 25, "exposuretime": 10, "exposuremode": 15, "irismode": 13, "irislevel": 16  }
   @{ "brightness": -999999, "contrast": -999999}\n
  */
#include "mediapipe_com.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <semaphore.h>
#define SERVER_PORT 8889
#define BUF_LEN 1024
#define DEFAULT_WIDTH "1920"
#define DEFAULT_HEIGHT "1080"
#define DEFAULT_FRAMERATE "30/1"
#define DEFAULT_ELEFORMAT "H264"
#define MAX_MESSAGE_LEN 1024
#define RTSP_LEN 60
#define PARAM_INT_FAILD -999999
#define PARAM_STR_FAILD "FAILED"
#define SERVERNAME "/tmp/onvif"

static gint client_socket;
static GHashTable *client_table = NULL;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t needProduct = PTHREAD_COND_INITIALIZER;

typedef void (*func)(struct json_object *, int *res_status, gchar *res_msg,
                     gpointer data);
struct route {
    char *name;
    func fun;
};

enum {
    RET_SUCESS = 0,
    RET_LENTH_EXCEEDING = -1,
    RET_COMMAND_ERROR = -2,
    RET_PARAM_JSON_ERROR = -3,
    RET_FAILED = -4
};

typedef enum {
    PARAM_STRING_TYPE = 0,
    PARAM_INT_TYPE = 1
} ParamType;

enum VideoPARAM {
    ENCODER = 0,
    WIDTH   = 1,
    HEIGHT,
    BITRATE,
    FRAMERATE,
    QUALITY,
    GOVLEN,
    VIDEO_PARAM_MAX
};

#define PARAM_STR_MAX_LEN  20
typedef struct {
    ParamType type;
    const char *name;
    gchar value_str[PARAM_STR_MAX_LEN];
    int value_int;
    gboolean status;
} Param;

//hash_table client data struct
typedef struct {
    gsize dataLenNeedHandle;/*the data length of need to handle*/
    gchar pData[MAX_MESSAGE_LEN];/*the container of client data*/
} ClientData;

typedef struct {
    mediapipe_t *mp;
    int quality;
    int bitrate;
    char *down_capsfilter_name;
    char *down_sink_name;
    int framerate;
    gchar format[20];
    int width;
    int height;
    int govlen;
} MediapipeChForm;

typedef struct {
    mediapipe_t *mp;
    const char *video_element;
    const char *image_element;
    const char *stream_uri;
    const char *videorate_capsfilter;
    int width;
    int height;
    GMainContext *context;
    GMainLoop *loop;
    const char *video_enc1;
    const char *video_enc2;
    const char *video_enc3;
} Context;

static Context *onvif_ctx = NULL ;
typedef struct {
    GstClockTime pts;
    gsize        buf_size;
    slice_type_t    slice_type;
} EncodeMeta;

static void check_param(Param *param, struct json_object *obj,
                        const char *debug_str);

static gboolean
get_element_by_name_and_direction(GstElement *cur_element,
                                  GstElement **ret_element,
                                  const char *ret_element_factory_name, gboolean isup);

static mp_int_t
init_module(mediapipe_t *mp);

static void exit_master(void);

static char *onvif_ctx_set(mediapipe_t *mp, mp_command_t *cmd);

static mp_command_t  mp_onvif_commands[] = {
    {
        mp_string("onvif"),
        MP_MAIN_CONF,
        onvif_ctx_set,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_module_ctx_t  mp_onvif_module_ctx = {
    mp_string("onvif"),
    NULL,
    NULL,
    NULL
};

mp_module_t  mp_onvif_module = {
    MP_MODULE_V1,
    &mp_onvif_module_ctx,                /* module context */
    mp_onvif_commands,                   /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                               /* init master */
    init_module,                               /* init module */
    NULL,                    /* keyshot_process*/
    NULL,                               /* message_process */
    NULL,                      /* init_callback */
    NULL,                               /* netcommand_process */
    exit_master,                               /* exit master */
    MP_MODULE_V1_PADDING
};



void close_fd_pointer (gpointer fd_pointer){
    int fd = GPOINTER_TO_INT(fd_pointer);
    close(fd);
}


static const char *get_element_name_from_mp_config(mediapipe_t *mp, const char *key_name)
{
     struct json_object *root = mp->config;
     struct json_object *parent;
     const char *value = NULL;

     if(!json_object_object_get_ex(root, "onvif", &parent))
     {
         LOG_WARNING("Config file have no \"onvif\" json object!");
         return NULL;
     }
     if(!json_get_string(parent, key_name, &value))
     {
        LOG_WARNING("Onvif json object of config file have no %s!",key_name);
        return NULL;
     }

     LOG_DEBUG("%s:%s",key_name,value);
     return value;
}

static int get_enc_num(char *str, char *delim)
{
    char s[20] = {'\0'};
    char *p = NULL;
    int num = -1;
    g_strlcpy(s, str, 19);
    strtok_r(s, delim, &p);
    num = atoi(p);
    return num;
}

static int _read_file(char *linebuffer, char *element, char  *elem_name)
{
    if(strstr(linebuffer, "mfxh264enc")) {
        strcpy(element, "H264");
        if(strstr(linebuffer, "name=enc0")) {
            strcpy(elem_name, "enc0");
        } else if(strstr(linebuffer, "name=enc1")) {
            strcpy(elem_name, "enc1");
        } else {
            strcpy(elem_name, "enc2");
        }
    } else if(strstr(linebuffer, "mfxjpegenc")) {
        strcpy(element, "JPEG");
        if(strstr(linebuffer, "name=enc0")) {
            strcpy(elem_name, "enc0");
        } else if(strstr(linebuffer, "name=enc1")) {
            strcpy(elem_name, "enc1");
        } else {
            strcpy(elem_name, "enc2");
        }
    } else {
        return -1;
    }
    return 0;
}

// rw 1 write,0 read
static int _read_write_file(char *file_path, char *element, char *elem_name,
                            int rw)
{
    char linebuffer[64] = {0};
    char save[64][64];
    memset(save, 0, sizeof(save));

    int i = 0, k = 0;

    FILE *fp = fopen(file_path, "rt+");
    if(fp == NULL) {
        LOG_ERROR("open %s error!", file_path);
        return -1;
    }

    while(fgets(save[i], 64, fp)) {
        i++;
    }

    fseek(fp, 0, SEEK_SET);
    while(fgets(linebuffer, 64, fp)) {
        if(rw) {
            if(strstr(linebuffer, elem_name)) {
                memset(linebuffer, '\0', strlen(linebuffer));
                sprintf(linebuffer, "%s\t\t\t\t%s !\n", element, elem_name);
                memset(save[k], '\0', strlen(save[k]));
                strcpy(save[k], linebuffer);

                fseek(fp, 0, SEEK_SET);
                truncate(file_path, 0);

                fseek(fp, 0, SEEK_SET);
                for(k = 0; k < i; k++) {
                    fwrite(save[k], strlen(save[k]), 1, fp);
                }
                break;
            }
        } else {
            if(!_read_file(linebuffer, element, elem_name)) {
                break;
            }
        }
        k++;
    }

    fclose(fp);
    return 0;
}
/* --------------------------------------------------------------------------*/
/**
 * @Synopsis it's a example of  extra function for change current encoder to 264
 *           format.  some extra  propertys  or some else  need to be changed
 *           after encoder changed , so do this here.
 *
 * @Param user_data
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static gboolean
change_to264(gpointer user_data)
{
    mediapipe_t *mp = (mediapipe_t *) user_data;
    int ret = 1;
    GstElement *enc3_caps = gst_bin_get_by_name(GST_BIN((mp)->pipeline),
                            "enc2_caps");
    GstCaps *caps =
        gst_caps_from_string("video/x-h264,stream-format=byte-stream,profile=high");
    gst_element_set_state(enc3_caps, GST_STATE_NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, "enc2_caps", "caps", caps, NULL);
    gst_element_set_state(enc3_caps, GST_STATE_PLAYING);
    gst_caps_unref(caps);
    gst_object_unref(enc3_caps);

    GstElement *sink = gst_bin_get_by_name(GST_BIN((mp)->pipeline), "sink2");
    gst_element_set_state(sink, GST_STATE_NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, "sink2", "location", "forchange.264", NULL);
    gst_element_set_state(sink, GST_STATE_PLAYING);
    gst_object_unref(sink);

    return (ret == 0);
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis it's a example of  extra function for change current encoder to 265
 *           format.  some extra  propertys  or some else  need to be changed
 *           after encoder changed , so do this here.
 *
 * @Param user_data
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static gboolean
change_to265(gpointer user_data)
{
    mediapipe_t *mp = (mediapipe_t *) user_data;
    int ret = 1;

    GstElement *enc3_caps = gst_bin_get_by_name(GST_BIN((mp)->pipeline),
                            "enc0_caps");
    GstCaps *caps =
        gst_caps_from_string("video/x-h265,stream-format=byte-stream,profile=high");
    gst_element_set_state(enc3_caps, GST_STATE_NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, "enc0_caps", "caps", caps, NULL);
    gst_element_set_state(enc3_caps, GST_STATE_PLAYING);
    gst_caps_unref(caps);
    gst_object_unref(enc3_caps);

    GstElement *sink = gst_bin_get_by_name(GST_BIN((mp)->pipeline), "sink0");
    gst_element_set_state(sink, GST_STATE_NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, "sink0", "location", "forchange.265", NULL);
    gst_element_set_state(sink, GST_STATE_PLAYING);
    gst_object_unref(sink);

    return (ret == 0);
}

static gboolean
change_format_in_channel(gpointer user_data)
{
    MediapipeChForm *mp_chform = (MediapipeChForm *) user_data;
    mediapipe_t *mp = mp_chform->mp;
    gchar *caps_name = mp_chform->down_capsfilter_name;
    gchar *format_s = mp_chform->format;
    int bitrate = mp_chform->bitrate;
    int quality = mp_chform->quality;
    int govlen = mp_chform->govlen;
    int framerate = mp_chform->framerate;
    const gchar *ele_name_s = onvif_ctx->video_element;
    const gchar *mount_path = onvif_ctx->stream_uri;
    gchar *sink_name = mp_chform->down_sink_name;
    int width = mp_chform->width;
    int height = mp_chform->height;

    //reset enc_caps and enc property
    GstElement *enc_caps_element = gst_bin_get_by_name(GST_BIN((mp)->pipeline),
                                   caps_name);
    if (enc_caps_element == NULL) {
        return FALSE;
    }
    GstCaps *caps = NULL;
    GstCaps *new_caps = NULL;
    GstStructure *structure = NULL;
    int ret = 0;
    gst_element_set_state(enc_caps_element, GST_STATE_NULL);
    if (!strcmp(format_s, "h265") || !strcmp(format_s, "H265")) {
        new_caps =
            gst_caps_from_string("video/x-h265,stream-format=byte-stream,alignment=au");
        MEDIAPIPE_SET_PROPERTY(ret, mp, ele_name_s , "preset", 6, NULL);
        MEDIAPIPE_SET_PROPERTY(ret, mp, ele_name_s , "idr-interval", 1, NULL);
        MEDIAPIPE_SET_PROPERTY(ret, mp, ele_name_s , "bitrate", bitrate, NULL);
        MEDIAPIPE_SET_PROPERTY(ret, mp, ele_name_s , "keyframe-period", govlen, NULL);
    } else if (!strcmp(format_s, "jpeg") || !strcmp(format_s, "JPEG")) {
        new_caps = gst_caps_from_string("image/jpeg");
        MEDIAPIPE_SET_PROPERTY(ret, mp, ele_name_s , "quality", quality, NULL);
    } else if (!strcmp(format_s, "h264") || !strcmp(format_s, "H264")) {
        new_caps =
            gst_caps_from_string("video/x-h264,stream-format=byte-stream,alignment=au");
        MEDIAPIPE_SET_PROPERTY(ret, mp, ele_name_s , "bitrate", bitrate, NULL);
        MEDIAPIPE_SET_PROPERTY(ret, mp, ele_name_s , "keyframe-period", govlen, NULL);
        MEDIAPIPE_SET_PROPERTY(ret, mp, ele_name_s , "rate-control", 1, NULL);
        MEDIAPIPE_SET_PROPERTY(ret, mp, ele_name_s , "resend-pps", 1, NULL);
        MEDIAPIPE_SET_PROPERTY(ret, mp, ele_name_s , "resend-sps", 1, NULL);
    }
    gst_object_unref(enc_caps_element);
    if (caps != NULL) {
        gst_caps_unref(caps);
    }
    MEDIAPIPE_SET_PROPERTY(ret, mp, caps_name , "caps", new_caps, NULL);
    if (ret != 0) {
        if (new_caps != NULL) {
            gst_caps_unref(caps);
        }
        return FALSE;
    }
    gst_element_set_state(enc_caps_element, GST_STATE_PLAYING);

    //prepare caps str for rtsp appsrc
    gchar r_caps_s[100] = {'\0'};
    gchar *tmp_caps_str =  gst_caps_to_string(new_caps);
    snprintf(r_caps_s, 100, "%s",  tmp_caps_str);
    g_free(tmp_caps_str);
    if (!strcmp(format_s, "jpeg") || !strcmp(format_s, "JPEG")) {
        sprintf(r_caps_s, "image/jpeg,width=%d,height=%d", width, height);
    }
    gst_caps_unref(new_caps);

    //reset sink
    GstElement *sink = gst_bin_get_by_name(GST_BIN((mp)->pipeline), sink_name);
    gst_element_set_state(sink, GST_STATE_NULL);
    if (sink == NULL) {
        return FALSE;
    }
    gst_element_set_state(sink, GST_STATE_PLAYING);
    gst_object_unref(sink);

    LOG_DEBUG("ele_name_s:%s", ele_name_s);
    LOG_DEBUG("r_caps_s:%s", r_caps_s);
    LOG_DEBUG("framerate:%d", framerate);
    LOG_DEBUG("mount_path:%s", mount_path);
    /*need send GstMessage*/
    /* mediapipe_remove_rtsp_mount_point(mp, mount_path); */
    /* mediapipe_rtsp_server_new (mp, ele_name_s, r_caps_s , 30, mount_path); */
    GstMessage *m;
    GstStructure *s;
    GstBus *test_bus = gst_element_get_bus(mp->pipeline);
    s = gst_structure_new("rtsp_restart", "ele_name_s", G_TYPE_STRING, ele_name_s,
                          "r_caps_s", G_TYPE_STRING, r_caps_s, "fps", G_TYPE_INT, framerate,
                          "mount_path", G_TYPE_STRING, mount_path, NULL);
    m = gst_message_new_application(NULL, s);
    gst_bus_post(test_bus, m);
    gst_object_unref(test_bus);
    g_free(mp_chform);
    pthread_cond_signal(&needProduct);
    return TRUE;
}

static void _set_videoenc_config(struct json_object *obj, int *res_status,
                                 gchar *res_msg, gpointer data)
{
    Context *ctx = (Context *) data;
    mediapipe_t *mp = ctx->mp;
    Param param[VIDEO_PARAM_MAX] = {0};
    param[ENCODER].type = PARAM_STRING_TYPE;
    param[ENCODER].name = "encoder";
    param[WIDTH].type = PARAM_INT_TYPE;
    param[WIDTH].name = "width";
    param[HEIGHT].type = PARAM_INT_TYPE;
    param[HEIGHT].name = "height";
    param[BITRATE].type = PARAM_INT_TYPE;
    param[BITRATE].name = "bitrate";
    param[FRAMERATE].type = PARAM_INT_TYPE;
    param[FRAMERATE].name = "framerate";
    param[QUALITY].type = PARAM_INT_TYPE;
    param[QUALITY].name = "quality";
    param[GOVLEN].type = PARAM_INT_TYPE;
    param[GOVLEN].name = "govlen";
    int ret = 0;
    *res_status = RET_SUCESS;

    //check param exist
    for (int i = 0; i < VIDEO_PARAM_MAX; i++) {
        check_param(&param[i], obj, "set video config");
    }
    
    if(param[WIDTH].value_int == 1280 && param[HEIGHT].value_int == 720 )
    {
        if(g_strcmp0(onvif_ctx->video_enc1, "720p") == 0)
        {
            ctx->video_element = "enc1";
            ctx->stream_uri = "/test1";
            ctx->videorate_capsfilter = "videorate1_caps";
            onvif_ctx->video_element = "enc1";
            onvif_ctx->stream_uri = "/test1";
        }
        else if(g_strcmp0(onvif_ctx->video_enc2, "720p") == 0)
        {
            ctx->video_element = "enc2";
            ctx->stream_uri = "/test2";
            ctx->videorate_capsfilter = "videorate2_caps";
            onvif_ctx->video_element = "enc2";
            onvif_ctx->stream_uri = "/test2";
        }
        else if(g_strcmp0(onvif_ctx->video_enc3, "720p") == 0)
        {
            ctx->video_element = "enc3";
            ctx->stream_uri = "/test3";
            ctx->videorate_capsfilter = "videorate3_caps";
            onvif_ctx->video_element = "enc3";
            onvif_ctx->stream_uri = "/test3";
        }
        else
        {
            LOG_INFO("please set video_enc1 in config file.\n");
        }
    }
    else if(param[WIDTH].value_int == 1920 && param[HEIGHT].value_int == 1080)
    {
        if(g_strcmp0(onvif_ctx->video_enc1, "1080p") == 0)
        {
            ctx->video_element = "enc1";
            ctx->stream_uri = "/test1";
            ctx->videorate_capsfilter = "videorate1_caps";
            onvif_ctx->video_element = "enc1";
            onvif_ctx->stream_uri = "/test1";
        }
        else if(g_strcmp0(onvif_ctx->video_enc2, "1080p") == 0)
        {
            ctx->video_element = "enc2";
            ctx->stream_uri = "/test2";
            ctx->videorate_capsfilter = "videorate2_caps";
            onvif_ctx->video_element = "enc2";
            onvif_ctx->stream_uri = "/test2";
        }
        else if(g_strcmp0(onvif_ctx->video_enc3, "1080p") == 0)
        {
            ctx->video_element = "enc3";
            ctx->stream_uri = "/test3";
            ctx->videorate_capsfilter = "videorate3_caps";
            onvif_ctx->video_element = "enc3";
            onvif_ctx->stream_uri = "/test3";
        }
        else
        {
            LOG_INFO("please set video_enc1 in config file.\n");
        }
    }
    else if(param[WIDTH].value_int == 3840 && param[HEIGHT].value_int == 2160)
    {
        if(g_strcmp0(onvif_ctx->video_enc1, "4k") == 0)
        {
            ctx->video_element = "enc1";
            ctx->stream_uri = "/test1";
            ctx->videorate_capsfilter = "videorate1_caps";
            onvif_ctx->video_element = "enc1";
            onvif_ctx->stream_uri = "/test1";
        }
        else if(g_strcmp0(onvif_ctx->video_enc2, "4k") == 0)
        {
            ctx->video_element = "enc2";
            ctx->stream_uri = "/test2";
            ctx->videorate_capsfilter = "videorate2_caps";
            onvif_ctx->video_element = "enc2";
            onvif_ctx->stream_uri = "/test2";
        }
        else if(g_strcmp0(onvif_ctx->video_enc3, "4k") == 0)
        {
            ctx->video_element = "enc3";
            ctx->stream_uri = "/test3";
            ctx->videorate_capsfilter = "videorate3_caps";
            onvif_ctx->video_element = "enc3";
            onvif_ctx->stream_uri = "/test3";
        }
        else
        {
            LOG_INFO("please set video_enc1 in config file.\n");
        }
    }
    else
    {
        LOG_ERROR("Resolution is not matched.");
    }
    ctx->width = param[WIDTH].value_int;
    ctx->height = param[HEIGHT].value_int;

    //get video element
    GstElement *enc_element = NULL;
    if (ctx->video_element != NULL) {
        enc_element = gst_bin_get_by_name(GST_BIN((mp)->pipeline),
                                          ctx->video_element);
    } else {
        LOG_WARNING("set video config: video element name is NULL");
        *res_status = RET_FAILED;
    }

    //set width and height
    if (param[WIDTH].status &&  param[WIDTH].value_int <= 0) {
        param[WIDTH].status = FALSE;
        *res_status = RET_FAILED;
        LOG_WARNING("set video config: error width :%d", param[WIDTH].value_int);
    }
    if (param[HEIGHT].status &&  param[HEIGHT].value_int <= 0) {
        param[HEIGHT].status = FALSE;
        *res_status = RET_FAILED;
        LOG_WARNING("set video config: error height :%d", param[HEIGHT].value_int);
    }
    GstElement *enc_up_casfilter_element =  NULL;
    GstCaps *caps = NULL;
    GstCaps *copy_caps = NULL;
    GstStructure *structure = NULL;
    GValue t_value = G_VALUE_INIT;
    g_value_init(&t_value, G_TYPE_INT);
    if (param[WIDTH].status && param[HEIGHT].status && enc_element != NULL) {
        if (get_element_by_name_and_direction(enc_element,
                                              &enc_up_casfilter_element, "capsfilter", TRUE)) {
            g_object_get(enc_up_casfilter_element, "caps", &caps, NULL);
            copy_caps = gst_caps_copy(caps);
            structure = gst_caps_get_structure(copy_caps, 0);
            g_value_set_int(&t_value, param[WIDTH].value_int);
            gst_structure_set_value(structure, "width", &t_value);
            g_value_set_int(&t_value, param[HEIGHT].value_int);
            gst_structure_set_value(structure, "height", &t_value);
            g_object_set(enc_up_casfilter_element, "caps", copy_caps, NULL);
            gst_caps_unref(caps);
            gst_caps_unref(copy_caps);
        } else {
            LOG_WARNING("set video config: can't find up capsfilter element");
            param[WIDTH].status = FALSE;
            param[HEIGHT].status = FALSE;
            *res_status = RET_FAILED;
        };
    }

    //set framerate
    if (param[FRAMERATE].status &&  param[FRAMERATE].value_int <= 0) {
        param[FRAMERATE].status = FALSE;
        *res_status = RET_FAILED;
        LOG_WARNING("set video config: error framerate :%d",
                    param[FRAMERATE].value_int);
    }

    if (param[FRAMERATE].status) {
        caps = NULL;
        copy_caps = NULL;
        structure = NULL;
        GstElement *videorate_capsfilter_element = NULL;
        g_value_unset(&t_value);
        g_value_init(&t_value, GST_TYPE_FRACTION);
        if (ctx->videorate_capsfilter != NULL) {
            videorate_capsfilter_element =
                gst_bin_get_by_name(GST_BIN((mp)->pipeline), ctx->videorate_capsfilter);
            g_object_get(videorate_capsfilter_element, "caps", &caps, NULL);
            copy_caps = gst_caps_copy(caps);
            structure = gst_caps_get_structure(copy_caps, 0);
            gst_value_set_fraction(&t_value, param[FRAMERATE].value_int, 1);
            gst_structure_set_value(structure, "framerate", &t_value);
            g_object_set(videorate_capsfilter_element, "caps", copy_caps, NULL);
            gst_caps_unref(caps);
            gst_caps_unref(copy_caps);
        } else {
            LOG_WARNING("set video config: videorate_capsfilter name is NULL");
            param[FRAMERATE].status = FALSE;
            *res_status = RET_FAILED;
        }
    }

    //change_encode;
    /*need send GstMessage*/
    /*mediapipe_change_format(mp, enc_name, queue_name, caps_name, _encoder,change_format_in_channel,mp_chform); */

    if (param[BITRATE].status &&  param[BITRATE].value_int < 0) {
        param[BITRATE].status = FALSE;
        *res_status = RET_FAILED;
        LOG_WARNING("set video config: error bitrate :%d", param[BITRATE].value_int);
    }
    if (param[GOVLEN].status &&  param[GOVLEN].value_int < 0) {
        param[GOVLEN].status = FALSE;
        *res_status = RET_FAILED;
        LOG_WARNING("set video config: error govlen :%d", param[GOVLEN].value_int);
    }
    if (param[QUALITY].status &&  param[QUALITY].value_int < 0) {
        param[QUALITY].status = FALSE;
        *res_status = RET_FAILED;
        LOG_WARNING("set video config: error quality  :%d", param[QUALITY].value_int);
    }
    if (param[ENCODER].status) {
        if (!strcmp(param[ENCODER].value_str, "JPEG")
            || !strcmp(param[ENCODER].value_str, "H264")
            || !strcmp(param[ENCODER].value_str, "H265")) {
        } else {
            param[ENCODER].status = FALSE;
            *res_status = RET_FAILED;
            LOG_WARNING("set video config: encoder error :%s", param[ENCODER].value_str);
        }
    }
    MediapipeChForm *mp_chform = g_new0(MediapipeChForm, 1);
    mp_chform->mp = mp;
    mp_chform->bitrate = param[BITRATE].value_int;
    mp_chform->govlen = param[GOVLEN].value_int;
    mp_chform->quality = param[QUALITY].value_int;
    mp_chform->framerate = param[FRAMERATE].value_int;
    sprintf(mp_chform->format, "%s", param[ENCODER].value_str);
    mp_chform->width = param[WIDTH].value_int;
    mp_chform->height = param[HEIGHT].value_int;
    GstElement  *sink = NULL;
    if (*res_status == RET_SUCESS) {
        if (get_element_by_name_and_direction(enc_element, &sink, "fakesink", FALSE)
            || get_element_by_name_and_direction(enc_element, &sink, "filesink", FALSE)) {
            g_object_get(sink, "name", &mp_chform->down_sink_name, NULL);
        } else {
            *res_status = RET_FAILED;
            param[ENCODER].status = FALSE;
        }
    }

    GstElement  *queue = NULL;
    GstElement  *down_capsfilter = NULL;
    if (*res_status == RET_SUCESS) {
        //get queue name and down_capsfilter and change format
        if (get_element_by_name_and_direction(enc_element, &queue, "queue", TRUE)
            && get_element_by_name_and_direction(enc_element,
                    &down_capsfilter, "capsfilter", FALSE)) {
            gchar *queue_name = NULL;
            gchar *caps_name = NULL;
            g_object_get(queue, "name", &queue_name, NULL);
            g_object_get(down_capsfilter, "name", &caps_name, NULL);
            mp_chform->down_capsfilter_name = caps_name;
            GstMessage *m;
            GstStructure *s;
            LOG_DEBUG("enc_name:%s", ctx->video_element);
            LOG_DEBUG("queue_name:%s", queue_name);
            LOG_DEBUG("caps_name:%s", caps_name);
            LOG_DEBUG("_encoder:%s", param[ENCODER].value_str);
            GstBus *test_bus = gst_element_get_bus(mp->pipeline);
            s = gst_structure_new("changeformat", "enc_name", G_TYPE_STRING,
                                  ctx->video_element, "queue_name",
                                  G_TYPE_STRING, queue_name, "caps_name", G_TYPE_STRING, caps_name, "_encoder",
                                  G_TYPE_STRING, param[ENCODER].value_str, "change_format_in_channel",
                                  G_TYPE_POINTER,
                                  change_format_in_channel, "mp_chform", G_TYPE_POINTER, mp_chform, NULL);
                                  
            m = gst_message_new_application(NULL, s);
            pthread_mutex_lock(&lock);
            gst_bus_post(test_bus, m);
            pthread_cond_wait(&needProduct, &lock);
            usleep(2000000);
            pthread_mutex_unlock(&lock);
            gst_object_unref(test_bus);
        } else {
            *res_status = RET_FAILED;
            param[ENCODER].status = FALSE;
        }
    }

    //param failed
    if (*res_status == RET_FAILED) {
        struct json_object *res_json_obj = json_object_new_object();
        for (int i = 0; i < VIDEO_PARAM_MAX; i++) {
            if (!param[i].status) {
                if (param[i].type == PARAM_STRING_TYPE) {
                    json_object_object_add(res_json_obj, param[i].name,
                                           json_object_new_string(PARAM_STR_FAILD));
                } else if (param[i].type == PARAM_INT_TYPE) {
                    json_object_object_add(res_json_obj, param[i].name,
                                           json_object_new_int(PARAM_INT_FAILD));
                }
            }
        }
        const char *str = json_object_to_json_string(res_json_obj);
        g_assert(strlen(str) < MAX_MESSAGE_LEN);
        sprintf(res_msg, "%s", str);
        json_object_put(res_json_obj);
    }
}

static void _set_image_config(struct json_object *obj, int *res_status,
                       gchar *res_msg, gpointer data)
{
    g_assert(obj != NULL);
    g_assert(res_status != NULL);
    g_assert(res_msg != NULL);
    g_assert(data != NULL);
    Context *ctx = (Context *) data;
    mediapipe_t *mp = ctx->mp;
    *res_status = 0;
    int ret = 0;
#define param_num  8
    const gchar *param[param_num] = {
        "brightness",
        "contrast",
        "colorsaturation",
        "sharpness",
        "exposuretime",
        "exposuremode",
        "irismode",
        "irislevel"
    };
    const gchar *property[param_num] = {
        "brightness",
        "contrast",
        "saturation",
        "sharpness",
        "exposure-time",
        "exp-priority",
        "iris-mode",
        "iris-level"
    };

    const char *_value = NULL;
    char *end = NULL;
    int default_value = -999999;
    int param_value[param_num] = {0};
    struct json_object *res_json_obj = json_object_new_object();
    struct json_object  *tmp_obj = NULL;
    gboolean param_status = TRUE;
    for (int i = 0 ; i < param_num; i++)
    {
        param_status = TRUE;
        param_value[i] = default_value;
        if (json_object_object_get_ex(obj, param[i], &tmp_obj)) {
            _value = json_object_get_string(tmp_obj);
            param_value[i] = g_ascii_strtoll(_value, &end, 10) ;
            if (_value == end && 0 == param_value[i]) {  // convert failed
                param_status = FALSE;
            } else if (ctx->image_element == NULL) {
                LOG_WARNING("set image config: but image element name is NULL");
            } else {
                MEDIAPIPE_SET_PROPERTY(ret, mp, ctx->image_element, property[i],
                           param_value[i], NULL);
                if (ret != 0) {
                    param_status = FALSE;
                }
            }
        } else {
            LOG_WARNING("param don't have '%s' \n", param[i]);
            param_status = FALSE;
        }
        if (!param_status) {
            param_value[i] = default_value;
            *res_status = RET_FAILED;
            json_object_object_add(res_json_obj, param[i],
                                   json_object_new_int(param_value[i]));
        }
    }
    const char *str = json_object_to_json_string(res_json_obj);
    g_assert(strlen(str) < MAX_MESSAGE_LEN);
    sprintf(res_msg, "%s", str);
    json_object_put(res_json_obj);
}

static int str_to_k(char *str, char *width, char *height, char *framerate)
{
    char delim[] = " ,;!/";
    char *p = NULL;
    char *w = NULL;
    char *h = NULL;
    char *f = NULL;
    for(p = strtok(str, delim); p != NULL; p = strtok(NULL, delim)) {
        if(strstr(p, "width")) {
            strtok_r(p, ")", &w);
            strcpy(width, w);
        } else if(strstr(p, "height")) {
            strtok_r(p, ")", &h);
            strcpy(height, h);
        } else if(strstr(p, "framerate")) {
            strtok_r(p, ")", &f);
            strcpy(framerate, f);
        }
    }
    return 0;
}

static void _get_videoenc_config(struct json_object *obj, int *res_status,
                                 gchar *res_msg, gpointer data)
{
    g_assert(obj != NULL);
    g_assert(res_status != NULL);
    g_assert(res_msg != NULL);
    g_assert(data != NULL);
    Context *ctx = (Context *) data;
    mediapipe_t *mp = ctx->mp;
    int ret = 0;

    //get element
    GstElement *enc_element = NULL;
    if (ctx->video_element != NULL) {
        enc_element = gst_bin_get_by_name(GST_BIN((mp)->pipeline),
                                          ctx->video_element);
    } else {
        LOG_WARNING("get video config: video element name is NULL");
    }

    //get enc src and sink caps
    GstPad *sink_pad = NULL;
    GstPad *src_pad = NULL;
    GstCaps *sink_caps = NULL;
    GstCaps *src_caps = NULL;
    if (enc_element != NULL) {
        sink_pad = gst_element_get_static_pad(enc_element, "sink");
        if (sink_pad == NULL) {
            sink_pad = gst_element_get_request_pad(enc_element, "sink%d");
        }
        if (sink_pad != NULL) {
            sink_caps = gst_pad_get_current_caps(sink_pad);
            gst_object_unref(sink_pad);
        }
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
        LOG_WARNING("get video config: pipeline do not have element :%s ",
                    ctx->video_element);
    }

    //get widht height
    gint _width = 1920;
    gint _height = 1080;
    char width[10] = {"1920"};
    char height[10] = {"1080"};
    gboolean ret2 = FALSE;
    GstVideoInfo sink_video_info;
    if (sink_caps != NULL) {
        ret2 = gst_video_info_from_caps(&sink_video_info, sink_caps);
        if (ret2) {
            _width = GST_VIDEO_INFO_WIDTH(&sink_video_info);
            _height = GST_VIDEO_INFO_HEIGHT(&sink_video_info);
        } else {
            LOG_WARNING("get video config:sink caps can not be parsed ");
        }
        gst_caps_unref(sink_caps);
    } else {
        LOG_WARNING("get video config: sink caps is NULL");
    }
    g_snprintf(width, 10, "%d", _width);
    g_snprintf(height, 10, "%d", _height);

    //get format
    char format[10] = {"H264"};
    GstStructure *src_caps_structure = NULL;
    if (src_caps != NULL) {
        src_caps_structure = gst_caps_get_structure(src_caps, 0);
        const char *name  = gst_structure_get_name(src_caps_structure);
        if (strstr(name, "h264")) {
            sprintf(format, "H264");
        } else if (strstr(name, "h265")) {
            sprintf(format, "H265");
        } else if (strstr(name, "jpeg")) {
            sprintf(format, "JPEG");
        }
        gst_caps_unref(src_caps);
    } else {
        LOG_WARNING("get video config: src caps is NULL");
    }

    //get quality or bitrate and bitratemode
    int  bitrate = 0;
    int  quality = 50;
    int  govlen = 30;
    int  _bitratemode = 1;
    char bitratemode[10] = {"CBR"};
    if (!strcmp(format, "JPEG")) {
        MEDIAPIPE_GET_PROPERTY(ret, mp, ctx->video_element, "quality",
                               &quality, NULL);
    } else {
        MEDIAPIPE_GET_PROPERTY(ret, mp, ctx->video_element, "bitrate",
                               &bitrate, NULL);
        MEDIAPIPE_GET_PROPERTY(ret, mp, ctx->video_element, "keyframe-period",
                               &govlen, NULL);
        MEDIAPIPE_GET_PROPERTY(ret, mp, ctx->video_element, "rate-control",
                               &_bitratemode, NULL);
        switch (_bitratemode) {
            case 1:
                sprintf(bitratemode, "CBR");
                break;
            case 2:
                sprintf(bitratemode, "VBR");
                break;
            case 3:
                sprintf(bitratemode, "CQP");
                break;
            default:
                sprintf(bitratemode, "CBR");
                break;
        }
    }

    //get framrate
    GstCaps *rate_caps = NULL;
    MEDIAPIPE_GET_PROPERTY(ret, mp, ctx->videorate_capsfilter, "caps",
                           &rate_caps, NULL);
    GstStructure *rate_caps_stucture = NULL;
    gint numerator = 30;
    gint denominator = 1;
    char framerate[10] = {"30/1"};
    if (ret == 0) {
        rate_caps_stucture = gst_caps_get_structure(rate_caps, 0);
        ret2 = gst_structure_get_fraction(rate_caps_stucture, "framerate",
                                          &numerator, &denominator);
        if (!ret2) {
            LOG_WARNING("fail to get value, use default value \"%s\"", framerate);
        } else {
            g_snprintf(framerate, 9, "%d/%d", numerator, denominator);
        }
        gst_caps_ref(rate_caps);
    }
    json_object_object_add(obj, "quality", json_object_new_int(quality));
    json_object_object_add(obj, "bitrate", json_object_new_int(bitrate));
    json_object_object_add(obj, "govlen", json_object_new_int(govlen));
    json_object_object_add(obj, "bitratemode", json_object_new_string(bitratemode));
    json_object_object_add(obj, "width", json_object_new_string(width));
    json_object_object_add(obj, "height", json_object_new_string(height));
    json_object_object_add(obj, "framerate", json_object_new_string(framerate));
    json_object_object_add(obj, "encoder", json_object_new_string(format));
    *res_status = 0;
}

static void _get_image_config(struct json_object *obj, int *res_status,
                       gchar *res_msg, gpointer data)
{
    g_assert(obj != NULL);
    g_assert(res_status != NULL);
    g_assert(res_msg != NULL);
    g_assert(data != NULL);
    Context *ctx = (Context *) data;
    mediapipe_t *mp = ctx->mp;

    int brightness = 0;
    int colorsaturation = 0;
    int contrast = 0;
    int sharpness = 0;
    int exposuretime = 0;
    int exposuremode = 0;
    int irismode = 0;
    int irislevel = 0;
    int ret = 0;

    if(ctx->image_element !=NULL ){
        MEDIAPIPE_GET_PROPERTY(ret, mp, ctx->image_element, "brightness", &brightness, NULL);
        MEDIAPIPE_GET_PROPERTY(ret, mp, ctx->image_element, "contrast", &contrast, NULL);
        MEDIAPIPE_GET_PROPERTY(ret, mp, ctx->image_element, "saturation", &colorsaturation, NULL);
        MEDIAPIPE_GET_PROPERTY(ret, mp, ctx->image_element, "sharpness", &sharpness, NULL);
        MEDIAPIPE_GET_PROPERTY(ret, mp, ctx->image_element, "exposure-time", &exposuretime, NULL);
        MEDIAPIPE_GET_PROPERTY(ret, mp, ctx->image_element, "exp-priority", &exposuremode, NULL);
        MEDIAPIPE_GET_PROPERTY(ret, mp, ctx->image_element, "iris-mode", &irismode, NULL);
        MEDIAPIPE_GET_PROPERTY(ret, mp, ctx->image_element, "iris-level", &irislevel, NULL);
    }else {
        LOG_WARNING("get image config: but image element name is NULL");
    }

    json_object_object_add(obj, "brightness", json_object_new_int(brightness));
    json_object_object_add(obj, "colorsaturation",
                           json_object_new_int(colorsaturation));
    json_object_object_add(obj, "contrast", json_object_new_int(contrast));
    json_object_object_add(obj, "sharpness", json_object_new_int(sharpness));
    json_object_object_add(obj, "exposuretime",
                           json_object_new_int(exposuretime));
    json_object_object_add(obj, "exposuremode",
                           json_object_new_int(exposuremode));
    json_object_object_add(obj, "irismode", json_object_new_int(irismode));
    json_object_object_add(obj, "irislevel", json_object_new_int(irislevel));

    *res_status = 0;
}

static void _get_range(struct json_object *obj, int *res_status, gchar *res_msg,
                gpointer data)
{
    g_assert(obj != NULL);
    g_assert(res_status != NULL);
    g_assert(res_msg != NULL);
    g_assert(data != NULL);

    Context *ctx = (Context *) data;
    mediapipe_t *mp = ctx->mp;

    int brightness_min = 0, brightness_max = 0;
    int colorsaturation_min = 0, colorsaturation_max = 0;
    int contrast_min = 0, contrast_max = 0;
    int sharpness_min = 0, sharpness_max = 0;
    int exposuretime_min = 0, exposuretime_max = 0;
    int iris_min = 0, iris_max = 0;

    GParamSpec *param = NULL;
    GParamSpecInt *pint = NULL;
    GstElement *element = NULL;
    if(ctx->image_element != NULL ){
        element = gst_bin_get_by_name(GST_BIN((mp)->pipeline), ctx->image_element);
    }else {
        LOG_WARNING("get range: image element  name is NULL ");
    }
    if (NULL == element) {
        LOG_WARNING("get_range : can't find src element");
    } else {
        param = g_object_class_find_property(G_OBJECT_GET_CLASS(element),
                "brightness");
        if (param != NULL) {
            pint = G_PARAM_SPEC_INT(param);
            if (pint != NULL) {
                brightness_min = pint->minimum;
                brightness_max = pint->maximum;
                pint = NULL;
            }
            param = NULL;
        } else {
            LOG_WARNING("get_range brightness failed");
        }
        param = g_object_class_find_property(G_OBJECT_GET_CLASS(element),
                "saturation");
        if (param != NULL) {
            pint = G_PARAM_SPEC_INT(param);
            if (pint != NULL) {
                colorsaturation_min = pint->minimum;
                colorsaturation_max = pint->maximum;
                pint = NULL;
            }
            param = NULL;
        } else {
            LOG_WARNING("get_range saturation failed");
        }
        param = g_object_class_find_property(G_OBJECT_GET_CLASS(element), "contrast");
        if (param != NULL) {
            pint = G_PARAM_SPEC_INT(param);
            if (pint != NULL) {
                contrast_min = pint->minimum;
                contrast_max = pint->maximum;
                pint = NULL;
            }
            param = NULL;
        } else {
            LOG_WARNING("get_range contrast failed");
        }
        param = g_object_class_find_property(G_OBJECT_GET_CLASS(element),
                "sharpness");
        if (param != NULL) {
            pint = G_PARAM_SPEC_INT(param);
            if (pint != NULL) {
                sharpness_min = pint->minimum;
                sharpness_max = pint->maximum;
                pint = NULL;
            }
            param = NULL;
        } else {
            LOG_WARNING("get_range sharpness failed");
        }
        param = g_object_class_find_property(G_OBJECT_GET_CLASS(element),
                "exposure-time");
        if (param != NULL) {
            pint = G_PARAM_SPEC_INT(param);
            if (pint != NULL) {
                exposuretime_min = pint->minimum;
                exposuretime_max = pint->maximum;
                pint = NULL;
            }
            param = NULL;
        } else {
            LOG_WARNING("get_range exposure-time failed");
        }
        param = g_object_class_find_property(G_OBJECT_GET_CLASS(element),
                "iris-level");
        if (param != NULL) {
            pint = G_PARAM_SPEC_INT(param);
            if (pint != NULL) {
                iris_min = pint->minimum;
                iris_max = pint->maximum;
                pint = NULL;
            }
            param = NULL;
        } else {
            LOG_WARNING("get_range iris-level failed");
        }
        gst_object_unref(element);
    }
    json_object_object_add(obj, "brightness_min",
                           json_object_new_int(brightness_min));
    json_object_object_add(obj, "brightness_max",
                           json_object_new_int(brightness_max));
    json_object_object_add(obj, "colorsaturation_min",
                           json_object_new_int(colorsaturation_min));
    json_object_object_add(obj, "colorsaturation_max",
                           json_object_new_int(colorsaturation_max));
    json_object_object_add(obj, "contrast_min",
                           json_object_new_int(contrast_min));
    json_object_object_add(obj, "contrast_max",
                           json_object_new_int(contrast_max));
    json_object_object_add(obj, "sharpness_min",
                           json_object_new_int(sharpness_min));
    json_object_object_add(obj, "sharpness_max",
                           json_object_new_int(sharpness_max));
    json_object_object_add(obj, "exposuretime_min",
                           json_object_new_int(exposuretime_min));
    json_object_object_add(obj, "exposuretime_max",
                           json_object_new_int(exposuretime_max));
    json_object_object_add(obj, "iris_min", json_object_new_int(iris_min));
    json_object_object_add(obj, "iris_max", json_object_new_int(iris_max));


    *res_status = 0;
}


static void _get_stream_uri(struct json_object *obj, int *res_status, gchar *res_msg,
                     gpointer data)
{
    Context *ctx = (Context *) data;
    mediapipe_t *mp = ctx->mp;
    gchar szRtsp[RTSP_LEN] = {'\0'};
    gint iRtspport = 8554;
    struct json_object* root = mp->config;
    struct json_object *rtsp;
    RETURN_IF_FAIL(json_object_object_get_ex(root, "rtsp",
                   &rtsp));
    gboolean has_port = json_get_int(rtsp, "rtsp_server_port", &iRtspport);
    if(!has_port) {
         LOG_WARNING("fail to get value(rtsp_server_port), use default value \"%d\"\n", iRtspport);
    }
    if (ctx->stream_uri != NULL) {
        g_snprintf(szRtsp, RTSP_LEN - 1, "rtsp://%s:%d%s", get_local_ip_addr(),
                    iRtspport, ctx->stream_uri);
    } else {
        g_snprintf(szRtsp, RTSP_LEN - 1, "rtsp://%s:%d/test0", get_local_ip_addr(),
                iRtspport);
    }
    json_object_object_add(obj,  "streamuri", json_object_new_string(szRtsp));
    *res_status = RET_SUCESS;
}



static struct route routes[] = {
    {"set_videoenc_config", _set_videoenc_config},
    {"set_image_config", _set_image_config},
    {"get_videoenc_config", _get_videoenc_config},
    {"get_image_config", _get_image_config},
    {"get_range", _get_range},
    {"get_stream_uri", _get_stream_uri}
};

static func get_func(char *name)
{
    // return routes[0].fun;
    for(int i = 0; i < sizeof(routes) / sizeof(struct route); i++) {
        if(strcmp(routes[i].name, name) == 0) {
            return routes[i].fun;
        }
    }
    return NULL;
}

static int parse(char *s, char *ope, struct json_object **param)
{
    g_assert(s != NULL);
    g_assert(ope != NULL);
    g_assert(param != NULL);
    const char *needle = "@";
    char *p = strstr(s, needle);
    if (p == NULL) {
        return RET_COMMAND_ERROR;
    }
    memcpy(ope, s, p - s);
    *(ope + (p - s)) = '\0';
    p++;
    *param = json_tokener_parse(p);
    if (*param == NULL) {
        return RET_PARAM_JSON_ERROR;
    }
    return 0;
}

static void handle(GIOChannel *gio, char *s, gpointer data)
{
    g_assert(gio != NULL);
    g_assert(s != NULL);
    g_assert(data != NULL);
    //    char *param;
    char operation[100] = {'\0'};
    gchar res[MAX_MESSAGE_LEN + 50] = {'\0'};
    gchar detail_res[MAX_MESSAGE_LEN] = {'\0'};
    struct json_object *param = NULL;
    int ret = parse(s, operation, &param);
    if (ret == 0) {
        func f = get_func(operation);
        if (f == NULL) {
            ret = RET_COMMAND_ERROR;
        } else {
            f(param, &ret, detail_res, data);
        }
    }
    /*return info format instruction:
     *1)SUCCESS info format:
     *@ if the command contains "get": command@info
     *@ else : success@0
     *2)ERROR info format: error@error_number@command_length@command@info
     *@about error_number:
     *@RET_COMMAND_ERROR: The command can not be identified.
     *@RET_PARAM_JSON_ERROR: The parameter's format of json is not right.
     *@RET_FAILED: Set failure.
     */
    switch (ret) {
        case RET_SUCESS: { //success
            /*eg:
             get_image_config@{ "brightness": -79, "colorsaturation": 0,
             "contrast": 0, "sharpness": 63, "exposuretime": 90,
             "exposuremode": 0, "irismode": 0, "irislevel": 0  }\n
            */
            if (strstr(operation, "get")) {
                sprintf(res, "%s@%s\n", operation, json_object_to_json_string(param));
            } else {
                //eg:success@0
                /* sprintf(res, "success@%d@%ld@%s", ret,  strlen(s), s); */
                sprintf(res, "success@%d\n", ret);
            }
            break;
        }
        case RET_FAILED: // failed
            /* eg:
              eg: error@-4@99@set_image_config@{ "brightness": 30, "contrast": 10,
              "colorsaturation": 20, "sharpness": 25, "exposuretime": 10,
              "exposuremode": 15, "irismode": 13, "irislevel": 16  }
              @{ "brightness": -999999, "contrast": -999999}\n
            */
            sprintf(res, "error@%d@%ld@%s", ret, strlen(s), s);
            res[strlen(res) - 1] = '\0';
            sprintf(res + strlen(res), "@%s\n",  detail_res);
            break;
        case RET_COMMAND_ERROR:   // command is not right
            /* eg:
              error@-2@164@ set_image_config1@{ "brightness":
              -79, "colorsaturation": 0, "contrast": 0, "sharpness": 63,
              "exposuretime": 90, "exposuremode": 0, "irismode": 0, "irislevel": 0  }\n
            */
        case RET_PARAM_JSON_ERROR:   // param json format is not right
            /* eg:
              error@-3@164@ set_image_config1@{ "brightness": -79,
              "colorsaturation": 0, "contrast": 0, "sharpness": 63,
              "exposuretime": 90, "exposuremode": 0, "irismode": 0, "irislevel": 0  }\n
            */
            sprintf(res, "error@%d@%ld@%s", ret, strlen(s), s);
            break;
        default:
            LOG_WARNING("unknow ret:%d input_command:%s\n", ret , s);
            break;
    }
    if (param != NULL) {
        json_object_put(param);
    }

    gint socket_fd = g_io_channel_unix_get_fd(gio);
    send(socket_fd, res, strlen(res), 0);
    LOG_DEBUG("Client %d: send data:%s", socket_fd, res);
    return ;
}

static gboolean gio_client_read_in_hanlder(GIOChannel *gio, GIOCondition condition,
                                    gpointer data)
{
    GIOStatus ret;
    GError *err = NULL;
    gsize read_len;
    char res[MAX_MESSAGE_LEN + 50] = {'\0'};

    if(condition & G_IO_HUP) {
        LOG_INFO("client Read end of pipe died !");
        return TRUE;
    }

    //get client fd
    gint client_socket_fd = g_io_channel_unix_get_fd(gio);

    //find client fd in hashtable
    ClientData *pClientData = (ClientData *)g_hash_table_lookup(client_table,
                          GINT_TO_POINTER(client_socket_fd));

    //not find
    if (NULL == pClientData) {
       pClientData = g_new0(ClientData, 1);
       pClientData->dataLenNeedHandle = 0;
       g_hash_table_insert(client_table, GINT_TO_POINTER(client_socket_fd), pClientData);
       LOG_DEBUG("A new client:%d", client_socket_fd);
   }

    //move to end
   gchar *pEndOfBuff = NULL;

    //read and handle
   while (1) {
       pEndOfBuff = pClientData->pData + pClientData->dataLenNeedHandle;
       //read
       ret = g_io_channel_read_chars(gio, pEndOfBuff,
                                     MAX_MESSAGE_LEN - pClientData->dataLenNeedHandle, &read_len, &err);
       //judge return value
       if (ret == G_IO_STATUS_ERROR || ret == G_IO_STATUS_EOF) {
           //close and remove client_socket_fd from hash table
           g_io_channel_shutdown(gio, TRUE, &err);
           g_hash_table_remove(client_table, GINT_TO_POINTER(client_socket_fd));
           LOG_DEBUG("Client %d: receive EOF or IO_STATUS_ERROR!", client_socket_fd);
           return FALSE;
       } else {
           //have readed all
           if (read_len == 0) {
               break;
           }
           char msg_temp[MAX_MESSAGE_LEN + 1] = {'\0'};
           memcpy(msg_temp, pEndOfBuff, read_len);
           LOG_DEBUG("Client %d: read_data: %s", client_socket_fd, msg_temp);
           //judge if the data contains the '\n'
           gchar *pHeadOfData = pClientData->pData;
           gchar *pointer = pClientData->pData + pClientData->dataLenNeedHandle;
           gchar *pHeadOfNewReadData = pointer;

           while (pointer - pHeadOfNewReadData < read_len) {
               if ((*pointer) == '\n') {
                   char _msg[MAX_MESSAGE_LEN] = {'\0'};
                   memcpy(_msg, pHeadOfData, (pointer - pHeadOfData + 1));
                   LOG_DEBUG("Client %d: Handle_Info: %s", client_socket_fd, _msg);
                   //handle command
                   handle(gio, _msg, data);
                   pHeadOfData = pointer + 1;
               }
               pointer ++;
           }
           pClientData->dataLenNeedHandle = pointer - pHeadOfData;
           if (pClientData->dataLenNeedHandle == MAX_MESSAGE_LEN) {
               //The data has exceed the maximum length of one ONVIF frame. Drop it.
               /*eg:
                error@-1@1620@ set_image_config@{ "brightness": -79, "colorsaturation": 0,
                "contrast": 0, "sharpness": 63, "exposuretime": 90, "exposuremode": 0,
                "irismode": 0, "irislevel": 0  }\n
               */
               sprintf(res, "error@%d@%d@", RET_LENTH_EXCEEDING, MAX_MESSAGE_LEN);
               int send_num = strlen(res);
               memcpy(res + strlen(res), pClientData->pData, MAX_MESSAGE_LEN);
               send_num += MAX_MESSAGE_LEN;
               res[send_num] = '\n';
               send_num++;
               send(client_socket_fd, res, send_num, 0);
               pClientData->dataLenNeedHandle = 0;
               LOG_DEBUG("Client %d: Hash buff is full!:%s", client_socket_fd, res);
           } else if(pClientData->pData == pHeadOfData) {
               //have no '\n',read again.
               continue;
           } else {
               //move the remain data of no contain '\n' to head of container.
               memmove(pClientData->pData,  pHeadOfData, pClientData->dataLenNeedHandle);
           }
       }
   }
    return TRUE;
}


static gboolean gio_client_in_handle(GIOChannel *gio, GIOCondition condition,
                                     gpointer data)
{
    Context *ctx = (Context *)data;
    GIOChannel *client_channel;

    gint socket_fd = g_io_channel_unix_get_fd(gio);
    if (socket_fd < 0) {
        perror("create socket failed");
    }
    if(condition & G_IO_HUP) {
        LOG_ERROR("Unexpected Broken pipe error on socket_fd !");
        close(socket_fd);
        return FALSE;
    }

    client_socket = accept(socket_fd, NULL, NULL);
    if(client_socket < 0) {
        LOG_ERROR("ERROR CLIENT_SOCKET VALUE !");
        return FALSE;
    }

    //find client fd in hashtable
    ClientData *pClientData = (ClientData *)g_hash_table_lookup(client_table,
                          GINT_TO_POINTER(client_socket));

    //not find
    if (NULL == pClientData) {
       pClientData = g_new0(ClientData, 1);
       pClientData->dataLenNeedHandle = 0;
       g_hash_table_insert(client_table, GINT_TO_POINTER(client_socket), pClientData);
       LOG_DEBUG("A new client:%d", client_socket_fd);
   }

    client_channel = g_io_channel_unix_new(client_socket);

    //set client socket none block #addFlag#
    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);

    // GIOCondition cond = ((GIOCondition) G_IO_IN|G_IO_HUP);
    GIOCondition cond = G_IO_IN;
    GSource * source= g_io_create_watch(client_channel, cond);
    g_source_set_callback(source, (GSourceFunc)gio_client_read_in_hanlder, data, NULL);
    g_assert(ctx->context != NULL);
    g_source_attach(source, ctx->context);
    g_io_channel_unref(client_channel);
    return TRUE;
}

static Context *create_context(mediapipe_t *mp)
{
    Context *ctx;
    ctx = g_new0(Context, 1);
    ctx->mp = mp;
    return ctx;
}

static char *onvif_ctx_set(mediapipe_t *mp, mp_command_t *cmd)
{
    onvif_ctx = create_context(mp);
    onvif_ctx->image_element = get_element_name_from_mp_config(mp, "image_element");
    onvif_ctx->video_element = get_element_name_from_mp_config(mp, "video_element");
    onvif_ctx->stream_uri = get_element_name_from_mp_config(mp, "stream_uri");
    onvif_ctx->videorate_capsfilter = get_element_name_from_mp_config(mp,
            "videorate_capsfilter");
    onvif_ctx->video_enc1 = get_element_name_from_mp_config(mp, "video_enc1");
    onvif_ctx->video_enc2 = get_element_name_from_mp_config(mp, "video_enc2");
    onvif_ctx->video_enc3 = get_element_name_from_mp_config(mp, "video_enc3");
    return MP_CONF_OK;
}

static void destroy_context(Context **ctx)
{
    g_free(*ctx);
}

static void *onvif_server_thread_run(void *data)
{
    GIOChannel *gio_socket_channel;
    gint server_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        perror("create socket failed");
    }
    struct sockaddr_un server_sockaddr;
    server_sockaddr.sun_family = AF_UNIX;
    strncpy(server_sockaddr.sun_path, SERVERNAME, sizeof(server_sockaddr.sun_path)-1);
    unlink(SERVERNAME);
    int reuse = 1;
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse,
                sizeof(reuse)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
    }

#ifdef SO_REUSEPORT
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEPORT, (const char *)&reuse,
                sizeof(reuse)) < 0) {
        perror("setsockopt(SO_REUSEPORT) failed");
    }
#endif

    if(bind(server_sockfd, (struct sockaddr *)&server_sockaddr,
            sizeof(server_sockaddr)) == -1) {
        perror("bind error");
        close(server_sockfd);
        exit(1);
    }

    if(listen(server_sockfd, 20) == -1) {
        perror("Communication listen error");
        close(server_sockfd);
        exit(1);
    }
    LOG_INFO("Communication server listen at: %d", SERVER_PORT);

    gio_socket_channel = g_io_channel_unix_new(server_sockfd);
    if(!gio_socket_channel) {
        g_error("Cannot create new Communication server GIOChannel!\n");
        exit(1);
    }
    //new client hash table use to contain client info
    if (NULL == client_table) {
        client_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, close_fd_pointer, g_free);
    }

    GIOCondition cond = G_IO_IN;
    GMainContext *context =  g_main_context_new();
    g_main_context_push_thread_default(context);
    GMainLoop *loop = g_main_loop_new(context, FALSE);
    GSource *source = g_io_create_watch(gio_socket_channel, cond);
    onvif_ctx->context = context;
    onvif_ctx->loop = loop;
    g_source_set_callback(source, (GSourceFunc)gio_client_in_handle, onvif_ctx, NULL);
    g_source_attach(source, context);
    g_io_channel_unref(gio_socket_channel);
    g_main_loop_run(loop);
    g_source_destroy(source);
    g_main_loop_unref(loop);
    g_main_context_unref(context);
    close(server_sockfd);
    g_hash_table_unref(client_table);
}

static void onvif_server_start(mediapipe_t *mp)
{
	GThread *p_thread;
	p_thread = g_thread_new("Unused String", onvif_server_thread_run, mp);
}


static mp_int_t
init_module(mediapipe_t *mp)
{
    onvif_server_start(mp);
    return MP_OK;
}

static void
exit_master(void)
{
    g_main_loop_quit(onvif_ctx->loop);
    destroy_context(&onvif_ctx);
}

static void check_param(Param *param, struct json_object *obj,
                        const char *debug_str)
{
    g_assert(param != NULL);
    g_assert(param->name != NULL);
    g_assert(obj != NULL);
    g_assert(debug_str != NULL);
    struct json_object *param_obj = NULL;
    const char *str = NULL;
    char *end = NULL;
    param->status = TRUE;
    if (param->type == PARAM_INT_TYPE) {
        if (!json_get_int(obj, param->name, &param->value_int)) {
            LOG_WARNING("%s : param error, has no '%s' param or param type in not int !",
                        debug_str, param->name);
            param->status = FALSE;
        }
        return;
    }
    if (param->type == PARAM_STRING_TYPE) {
        if (!json_get_string(obj, param->name, &str)) {
            LOG_WARNING("%s : param error, has no '%s' param or param type in not string !",
                        debug_str, param->name);
            param->status = FALSE;
            return;
        }
        if (strlen(str) >= PARAM_STR_MAX_LEN) {
            LOG_WARNING("%s : param error, '%s' is too length,max: %d, now: %ld !",
                        debug_str, param->name, PARAM_STR_MAX_LEN, strlen(str));
            param->status = FALSE;
            return;
        } else {
            sprintf(param->value_str, "%s", str);
        }
    }
}

static gboolean
get_element_by_name_and_direction(GstElement *cur_element,
                                  GstElement **ret_element,
                                  const char *ret_element_factory_name, gboolean isup)
{
    g_assert(cur_element != NULL);
    g_assert(ret_element_factory_name != NULL);
    GstPad *other_pad = NULL;
    GstElement *element = NULL;
    gchar *element_name = NULL;
    gboolean ret = FALSE;
    GstPad *pad = NULL;
    if (isup) {
        pad = gst_element_get_static_pad(cur_element, "sink");
    } else {
        pad = gst_element_get_static_pad(cur_element, "src");
    }
    for (;;) {
        other_pad = gst_pad_get_peer(pad);
        gst_object_unref(pad);
        if (!other_pad) {
            break;
        }
        element = gst_pad_get_parent_element(other_pad);
        gst_object_unref(other_pad);
        if (!element) {
            break;
        }
        element_name =
            gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(gst_element_get_factory(
                                            element)));
        if (!element_name) {
            break;
        }
        if (0 == strcmp(element_name, ret_element_factory_name)) {
            ret = TRUE;
            break;
        } else  {
            if (isup) {
                pad = gst_element_get_static_pad(element, "sink");
            } else {
                pad = gst_element_get_static_pad(element, "src");
            }
            if (!pad) {
                break;
            }
        }
        element_name = NULL;
        g_clear_object(&element);
    }
    if (ret) {
        *ret_element = gst_object_ref(element);
    } else {
        LOG_WARNING("can't get element :%s, from  %s", ret_element_factory_name,
                    isup ? "up" : "down");
    }
    g_clear_object(&element);
    return ret;
}
