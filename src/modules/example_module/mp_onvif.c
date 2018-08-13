#include "mediapipe_com.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>

#define SERVER_PORT 8889
#define BUF_LEN 1024
#define DEFAULT_WIDTH "1920"
#define DEFAULT_HEIGHT "1080"
#define DEFAULT_FRAMERATE "30/1"
#define DEFAULT_ELEFORMAT "H264"
#define MAX_MESSAGE_LEN 1024


static char *file_path = "/etc/mediapipe/launch.txt";
static char elem_format[20] = {'\0'};
static char enc_name[20] = {'\0'};
static GHashTable *client_table = NULL;

typedef void (*func)(struct json_object *, int *res_status, gchar **res_msg,
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

//hash_table client data struct
typedef struct {
    gsize dataLenNeedHandle;/*the data length of need to handle*/
    gchar pData[MAX_MESSAGE_LEN];/*the container of client data*/
} ClientData;

typedef struct {
    mediapipe_t *mp;
    struct json_object *js_obj;
} MediapipeChForm;

typedef struct {
    mediapipe_t *mp;
    const char *video_element;
    const char *image_element;
    const char *stream_uri;
} Context;

static Context *onvif_ctx = NULL ;
typedef struct {
    GstClockTime pts;
    gsize        buf_size;
    slice_type_t    slice_type;
} EncodeMeta;

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

static mp_core_module_t  mp_onvif_module_ctx = {
    mp_string("onvif"),
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
    strcpy(s, str);
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
    struct json_object *js_obj = mp_chform->js_obj;
    int ret = 1;

    json_object *ele_name_obj = NULL;
    json_object *e_caps_value_obj = NULL;
    json_object *e_caps_name_obj = NULL;
    json_object *mount_path_obj = NULL;
    json_object *format_obj = NULL;
    json_object *r_caps_obj = NULL;

    if(!json_object_object_get_ex(js_obj, "ele_name", &ele_name_obj)) {
        return 1;
    }
    if(!json_object_object_get_ex(js_obj, "e_caps_value", &e_caps_value_obj)) {
        return 1;
    }
    if(!json_object_object_get_ex(js_obj, "e_caps_name", &e_caps_name_obj)) {
        return 1;
    }
    if(!json_object_object_get_ex(js_obj, "mount_path", &mount_path_obj)) {
        return 1;
    }
    if(!json_object_object_get_ex(js_obj, "format", &format_obj)) {
        return 1;
    }
    if(!json_object_object_get_ex(js_obj, "r_caps", &r_caps_obj)) {
        return 1;
    }

    const char *ele_name_s = json_object_get_string(ele_name_obj);
    const char *e_caps_value_s = json_object_get_string(e_caps_value_obj);
    const char *e_caps_name_s = json_object_get_string(e_caps_name_obj);
    const char *mount_path = json_object_get_string(mount_path_obj);
    const char *format_s = json_object_get_string(format_obj);
    const char *r_caps_s = json_object_get_string(r_caps_obj);

    GstElement *enc_caps = gst_bin_get_by_name(GST_BIN((mp)->pipeline),
                           e_caps_name_s);
    GstCaps *caps = gst_caps_from_string(e_caps_value_s);
    gst_element_set_state(enc_caps, GST_STATE_NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, e_caps_name_s, "caps", caps, NULL);
    if(!strcmp(format_s, "h265") || !strcmp(format_s, "H265")) {
        MEDIAPIPE_SET_PROPERTY(ret, mp, ele_name_s , "preset", 6, NULL);
        MEDIAPIPE_SET_PROPERTY(ret, mp, ele_name_s , "idr-interval", 1, NULL);
    } else if(!strcmp(format_s, "jpeg") || !strcmp(format_s, "JPEG")) {
        MEDIAPIPE_SET_PROPERTY(ret, mp, ele_name_s , "quality", 50, NULL);
    }
    gst_element_set_state(enc_caps, GST_STATE_PLAYING);
    gst_caps_unref(caps);
    gst_object_unref(enc_caps);

    char sinK_pad[20] = {'\0'};
    if(!strcmp(ele_name_s, "enc0")) {
        strcpy(sinK_pad, "sink0");
    } else if(!strcmp(ele_name_s, "enc1")) {
        strcpy(sinK_pad, "sink1");
    } else {
        strcpy(sinK_pad, "sink2");
    }

    GstElement *sink = gst_bin_get_by_name(GST_BIN((mp)->pipeline), sinK_pad);
    gst_element_set_state(sink, GST_STATE_NULL);
    gst_element_set_state(sink, GST_STATE_PLAYING);
    gst_object_unref(sink);

    /*need send GstMessage*/
    /* mediapipe_remove_rtsp_mount_point(mp, mount_path); */
    /* mediapipe_rtsp_server_new (mp, ele_name_s, r_caps_s , 30, mount_path); */

    GstMessage *m;
    GstStructure *s;
    GstBus * test_bus = gst_element_get_bus(mp->pipeline);

    s = gst_structure_new ("rtsp_restart","ele_name_s", G_TYPE_STRING, ele_name_s,
        "r_caps_s", G_TYPE_STRING, r_caps_s, "fps", G_TYPE_INT, 30,
        "mount_path", G_TYPE_STRING, mount_path, NULL);
    m = gst_message_new_application (NULL, s);
    gst_bus_post(test_bus, m);
    gst_object_unref(test_bus);

    return (ret == 0);
}

static void _set_videoenc_config(struct json_object *obj, int *res_status,
                          gchar **res_msg, gpointer data)
{

    Context *ctx = (Context *) data;
    mediapipe_t *mp = ctx->mp;

    GstCaps *caps;
    int ret = 0;

    json_object *encoder = NULL;
    json_object *width = NULL;
    json_object *height = NULL;
    json_object *bitrate = NULL;
    json_object *framerate = NULL;
    json_object *quality = NULL;

    if(!json_object_object_get_ex(obj, "encoder", &encoder)) {
        LOG_ERROR("param error, has no \"encoder\" element !");
        *res_status = -1;
        *res_msg = strdup("param error, no has element name\n");
        return ;
    }
    if(!json_object_object_get_ex(obj, "width", &width)) {
        LOG_ERROR("param error, has no \"width\" element !");
        *res_status = -1;
        *res_msg = strdup("param error, no has element name\n");
        return ;
    }
    if(!json_object_object_get_ex(obj, "height", &height)) {
        LOG_ERROR("param error, has no \"height\" element !");
        *res_status = -1;
        *res_msg = strdup("param error, no has element name\n");
        return ;
    }
    if(!json_object_object_get_ex(obj, "framerate", &framerate)) {
        LOG_ERROR("param error, has no \"framerate\" element !");
    }
    if(!json_object_object_get_ex(obj, "quality", &quality)) {
        LOG_ERROR("param error, has no \"quality\" element !");
    }
    if(!json_object_object_get_ex(obj, "bitrate", &bitrate)) {
        LOG_ERROR("param error, has no \"bitrate\" element !");
    }

    const char *_encoder = json_object_get_string(encoder);
    const char *_width = json_object_get_string(width);
    const char *_height = json_object_get_string(height);
    const char *_framerate = json_object_get_string(framerate);
    const char *_quality = json_object_get_string(quality);
    const char *_bitrate = json_object_get_string(bitrate);
    char buf[BUF_LEN] = {'\0'};

    if(!strcmp(elem_format, _encoder)) {
        if(!strcmp(_encoder, "JPEG") || !strcmp(_encoder, "jpeg")) {
            MEDIAPIPE_SET_PROPERTY(ret, mp, enc_name, "quality", atoi(_quality), NULL);
        } else {
            MEDIAPIPE_SET_PROPERTY(ret, mp, enc_name, "bitrate", atoi(_bitrate), NULL);
        }
    } else {

        memset(elem_format, '\0', 20);
        strcpy(elem_format, _encoder);

        MediapipeChForm *mp_chform = g_new0(MediapipeChForm, 1);
        struct json_object *js_obj = json_object_new_object();
        mp_chform->mp = mp;

        char caps_name[20] = {'\0'};
        char mount_path[20] = {'\0'};
        char queue_name[20] = {'\0'};

        sprintf(caps_name, "%s_caps", enc_name);
        sprintf(mount_path, "/test%d", get_enc_num(enc_name, "c"));
        sprintf(queue_name, "qfc%d", get_enc_num(enc_name, "c"));

        json_object_object_add(js_obj, "ele_name", json_object_new_string(enc_name));
        json_object_object_add(js_obj, "format", json_object_new_string(_encoder));
        json_object_object_add(js_obj, "mount_path",
                               json_object_new_string(mount_path));
        json_object_object_add(js_obj, "e_caps_name",
                               json_object_new_string(caps_name));

        if(!strcmp(_encoder, "JPEG") || !strcmp(_encoder, "jpeg")) {
            memset(buf, '\0', sizeof(buf));
            sprintf(buf, "image/jpeg,width=%s,height=%s", _width, _height);
            json_object_object_add(js_obj, "e_caps_value",
                                   json_object_new_string("image/jpeg"));
            json_object_object_add(js_obj, "r_caps", json_object_new_string(buf));
        } else {
            json_object_object_add(js_obj, "e_caps_value",
                                   json_object_new_string("video/x-h264,stream-format=byte-stream,profile=high"));
            json_object_object_add(js_obj, "r_caps",
                                   json_object_new_string("video/x-h264,stream-format=byte-stream,alignment=au"));
        }

        mp_chform->js_obj = js_obj;
        /*need send GstMessage*/
        /*mediapipe_change_format(mp, enc_name, queue_name, caps_name, _encoder,change_format_in_channel,mp_chform);*/
        GstMessage *m;
        GstStructure *s;
        GstBus * test_bus = gst_element_get_bus(mp->pipeline);

        s = gst_structure_new ("changeformat","enc_name", G_TYPE_STRING, enc_name, "queue_name",
            G_TYPE_STRING, queue_name, "caps_name", G_TYPE_STRING, caps_name, "_encoder",
            G_TYPE_STRING, _encoder, "change_format_in_channel", G_TYPE_POINTER,
            change_format_in_channel, "mp_chform", G_TYPE_POINTER, mp_chform, NULL);
        m = gst_message_new_application (NULL, s);
        gst_bus_post(test_bus, m);
        gst_object_unref(test_bus);

        if(!strcmp(_encoder, "JPEG") || !strcmp(_encoder, "jpeg")) {
            MEDIAPIPE_SET_PROPERTY(ret, mp, enc_name, "quality", atoi(_quality), NULL);
        } else {
            MEDIAPIPE_SET_PROPERTY(ret, mp, enc_name, "bitrate", atoi(_bitrate), NULL);
        }

    }

    memset(buf, '\0', sizeof(buf));
    sprintf(buf,
            "video/x-raw(memory:MFXSurface),format=NV12,width=%s,height=%s,framerate=%s/1",
            _width, _height, _framerate);
    char capsfilter[20] = {'\0'};
    sprintf(capsfilter, "scale%d_mfx_caps", get_enc_num(enc_name, "c"));
    caps = gst_caps_from_string(buf);
    MEDIAPIPE_SET_PROPERTY(ret, mp, capsfilter, "caps", caps, NULL);
    gst_caps_unref(caps);



    *res_status = 0;
}

static void _set_image_config(struct json_object *obj, int *res_status,
                       gchar **res_msg, gpointer data)
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
            } else {
                MEDIAPIPE_SET_PROPERTY(ret, mp, "src", property[i], param_value[i], NULL);
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
    g_assert(strlen(str) <= MAX_MESSAGE_LEN);
    sprintf(*res_msg, "%s", str);
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
                          gchar **res_msg, gpointer data)
{
    g_assert(obj != NULL);
    g_assert(res_status != NULL);
    g_assert(res_msg != NULL);
    g_assert(data != NULL);
    Context *ctx = (Context *) data;
    mediapipe_t *mp = ctx->mp;

    GstCaps *caps_tmp = NULL;
    gchar *caps_type = NULL;
    int  bitrate = 0;
    int  ret = 0;
    int  quality = 50;

    if(enc_name[0] != '\0') {
        char caps_name[20] = {'\0'};
        sprintf(caps_name, "%s_caps", enc_name);
        MEDIAPIPE_GET_PROPERTY(ret, mp, caps_name, "caps", &caps_tmp, NULL);
        caps_type = gst_caps_to_string(caps_tmp);
        if(strstr(caps_type, "h264")) {
            strcpy(elem_format, "H264");
        } else if(strstr(caps_type, "jpeg")) {
            strcpy(elem_format, "JPEG");
        } else {
            LOG_ERROR("Can't get the required channel !");
            return ;
        }

    } else {
        if(_read_write_file(file_path, elem_format, enc_name, 0)) {
            LOG_ERROR("Can't get the required channel !");
            return ;
        }
    }

    if(!strcmp(elem_format, "JPEG")) {
        MEDIAPIPE_GET_PROPERTY(ret, mp, enc_name, "quality", &quality, NULL);
    } else {
        MEDIAPIPE_GET_PROPERTY(ret, mp, enc_name, "bitrate", &bitrate, NULL);
    }

    char capsfilter[20] = {'\0'};
    sprintf(capsfilter, "scale%d_mfx_caps", get_enc_num(enc_name, "c"));
    MEDIAPIPE_GET_PROPERTY(ret, mp, capsfilter, "caps", &caps_tmp, NULL);
    caps_type = gst_caps_to_string(caps_tmp);

    gchar width[10] = {DEFAULT_WIDTH};
    gchar height[10] = {DEFAULT_HEIGHT};
    gchar framerate[10] = {DEFAULT_FRAMERATE};
    GstStructure *s = gst_caps_get_structure (caps_tmp, 0);

    gint _width = 0;
    gboolean ret2 = gst_structure_get_int (s,"width", &_width);
    if(!ret2) {
        LOG_WARNING("fail to get value, use default value \"%s\"", width);
    } else {
        sprintf(width, "%d", _width);
    }
    gint _height = 0;
    ret2 = gst_structure_get_int(s, "height", &_height);
    if(!ret2) {
        LOG_WARNING("fail to get value, use default value \"%s\"", height);
    } else {
        sprintf(height, "%d", _height);
    }

    gint numerator = 0;
    gint denominator = 0;
    ret2 = gst_structure_get_fraction(s, "framerate", &numerator, &denominator);
    if(!ret2) {
        LOG_WARNING("fail to get value, use default value \"%s\"", framerate);
    } else {
        sprintf(framerate, "%d/%d", numerator, denominator);
    }
    check_config_value(elem_format, DEFAULT_ELEFORMAT);

    json_object_object_add(obj, "quality", json_object_new_int(quality));
    json_object_object_add(obj, "bitrate", json_object_new_int(bitrate));
    json_object_object_add(obj, "bitratemode", json_object_new_string("BR"));
    json_object_object_add(obj, "width", json_object_new_string(width));
    json_object_object_add(obj, "height", json_object_new_string(height));
    json_object_object_add(obj, "framerate", json_object_new_string(framerate));
    json_object_object_add(obj, "encoder", json_object_new_string(elem_format));


    *res_status = 0;

}

static void _get_image_config(struct json_object *obj, int *res_status,
                       gchar **res_msg, gpointer data)
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

    MEDIAPIPE_GET_PROPERTY(ret, mp, "src", "brightness", &brightness, NULL);
    MEDIAPIPE_GET_PROPERTY(ret, mp, "src", "contrast", &contrast, NULL);
    MEDIAPIPE_GET_PROPERTY(ret, mp, "src", "saturation", &colorsaturation, NULL);
    MEDIAPIPE_GET_PROPERTY(ret, mp, "src", "sharpness", &sharpness, NULL);
    MEDIAPIPE_GET_PROPERTY(ret, mp, "src", "exposure-time", &exposuretime, NULL);
    MEDIAPIPE_GET_PROPERTY(ret, mp, "src", "exp-priority", &exposuremode, NULL);
    MEDIAPIPE_GET_PROPERTY(ret, mp, "src", "iris-mode", &irismode, NULL);
    MEDIAPIPE_GET_PROPERTY(ret, mp, "src", "iris-level", &irislevel, NULL);

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

static void _get_range(struct json_object *obj, int *res_status, gchar **res_msg,
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
    GstElement *element = gst_bin_get_by_name(GST_BIN((mp)->pipeline), ("src"));
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


static void _get_stream_uri(struct json_object *obj, int *res_status, gchar **res_msg,
                     gpointer data)
{
    g_assert(obj != NULL);
    g_assert(res_status != NULL);
    g_assert(res_msg != NULL);
    g_assert(data != NULL);
    char streamuri[20] = {'\0'};
    sprintf(streamuri, "test%d", get_enc_num(enc_name, "c"));
    json_object_object_add(obj, "streamuri", json_object_new_string(streamuri));

    *res_status = 0;
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
    gchar *detail_res_pointer = detail_res;
    struct json_object *param = NULL;
    int ret = parse(s, operation, &param);
    if (ret == 0) {
        func f = get_func(operation);
        if (f == NULL) {
            ret = RET_COMMAND_ERROR;
        } else {
            f(param, &ret, &detail_res_pointer, data);
        }
    }
    switch (ret) {
        case RET_SUCESS: { //success
            if (strstr(operation, "get")) {
                sprintf(res, "%s@%s\n", operation, json_object_to_json_string(param));
            } else {
                /* sprintf(res, "success@%d@%ld@%s", ret,  strlen(s), s); */
                sprintf(res, "success@%d\n", ret);
            }
            break;
        }
        case RET_FAILED: // failed
            sprintf(res, "error@%d@%ld@%s", ret, strlen(s), s);
            res[strlen(res) - 1] = '\0';
            sprintf(res + strlen(res), "@%s\n",  detail_res);
            break;
        case RET_COMMAND_ERROR:   // command is not right
        case RET_PARAM_JSON_ERROR:   // param json format is not right
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
                          &client_socket_fd);

    //not find
    if (NULL == pClientData) {
       gint *pClient_socket_fd = g_new0(gint, 1);
       *pClient_socket_fd = client_socket_fd;
       pClientData = g_new0(ClientData, 1);
       pClientData->dataLenNeedHandle = 0;
       g_hash_table_insert(client_table, pClient_socket_fd, pClientData);
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
           close(client_socket_fd);
           g_hash_table_remove(client_table, &client_socket_fd);
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

    GIOChannel *client_channel;
    gint client_socket;

    gint socket_fd = g_io_channel_unix_get_fd(gio);

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

    client_channel = g_io_channel_unix_new(client_socket);

    //set client socket none block #addFlag#
    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);

    // GIOCondition cond = ((GIOCondition) G_IO_IN|G_IO_HUP);
    GIOCondition cond = G_IO_IN;
    g_io_add_watch(client_channel, cond, (GIOFunc) gio_client_read_in_hanlder,
                   data);
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

    return MP_CONF_OK;
}

static void destroy_context(Context **ctx)
{
    g_free(*ctx);
}

static void onvif_server_start(mediapipe_t *mp)
{
    GIOChannel *gio_socket_channel;
    gint server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_sockaddr;
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port = htons(SERVER_PORT);
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

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

    if(bind(server_sockfd, (struct sockaddr *) &server_sockaddr,
            sizeof(server_sockaddr)) == -1) {
        perror("bind error");
        exit(1);
    }

    if(listen(server_sockfd, 20) == -1) {
        perror("Communication listen error");
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
        client_table = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, g_free);
    }
    GIOCondition cond = G_IO_IN;
    g_io_add_watch(gio_socket_channel, cond, (GIOFunc) gio_client_in_handle, onvif_ctx);
    g_io_channel_unref(gio_socket_channel);
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
    g_hash_table_unref(client_table);
    destroy_context(&onvif_ctx);
}
