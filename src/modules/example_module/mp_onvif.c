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
#include "gstocl/oclcommon.h"

#define SERVER_PORT 8889
#define BUF_LEN 1024
#define DEFAULT_WIDTH "1920"
#define DEFAULT_HEIGHT "1080"
#define DEFAULT_FRAMERATE "30/1"
#define DEFAULT_ELEFORMAT "H264"
static char *file_path = "/etc/mediapipe/launch.txt";
static char elem_format[20] = {'\0'};
static char enc_name[20] = {'\0'};

typedef void (*func)(struct json_object *, int *res_status, gchar **res_msg,
                     gpointer data);
struct route {
    char *name;
    func fun;
};

typedef struct {
    mediapipe_t *mp;
    struct json_object *js_obj;
} MediapipeChForm;

typedef struct {
    mediapipe_t *mp;
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

static mp_command_t  mp_onvif_commands[] = {
    {
        mp_string("onvif"),
        MP_MAIN_CONF,
        NULL,
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
        printf("open error");
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
        printf("param error, no has element name \n");
        *res_status = -1;
        *res_msg = strdup("param error, no has element name\n");
        return ;
    }
    if(!json_object_object_get_ex(obj, "width", &width)) {
        printf("param error, no has element name \n");
        *res_status = -1;
        *res_msg = strdup("param error, no has element name\n");
        return ;
    }
    if(!json_object_object_get_ex(obj, "height", &height)) {
        printf("param error, no has element name \n");
        *res_status = -1;
        *res_msg = strdup("param error, no has element name\n");
        return ;
    }
    if(!json_object_object_get_ex(obj, "framerate", &framerate)) {
        printf("param error, no has element name \n");
    }
    if(!json_object_object_get_ex(obj, "quality", &quality)) {
        printf("param error, no has element name \n");
    }
    if(!json_object_object_get_ex(obj, "bitrate", &bitrate)) {
        printf("param error, no has element name \n");
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



    *res_status = 1;
    *res_msg = strdup("_set_videoenc_config success\n");
}

static void _set_image_config(struct json_object *obj, int *res_status,
                       gchar **res_msg, gpointer data)
{
    Context *ctx = (Context *) data;
    mediapipe_t *mp = ctx->mp;
    int ret = 0;

    json_object *brightness = NULL;
    json_object *contrast = NULL;
    json_object *colorsaturation = NULL;
    json_object *sharpness = NULL;
    json_object *exposuretime = NULL;
    json_object *exposuremode = NULL;
    json_object *irismode = NULL;
    json_object *irislevel = NULL;

    if(!json_object_object_get_ex(obj, "brightness", &brightness)) {
        printf("param error, no has element name \n");
        *res_status = -1;
        *res_msg = strdup("param error, no has element name\n");
        return ;
    }
    if(!json_object_object_get_ex(obj, "contrast", &contrast)) {
        printf("param error, no has element name \n");
        *res_status = -1;
        *res_msg = strdup("param error, no has element name\n");
        return ;
    }
    if(!json_object_object_get_ex(obj, "colorsaturation", &colorsaturation)) {
        printf("param error, no has element name \n");
        *res_status = -1;
        *res_msg = strdup("param error, no has element name\n");
        return ;
    }
    if(!json_object_object_get_ex(obj, "sharpness", &sharpness)) {
        printf("param error, no has element name \n");
        *res_status = -1;
        *res_msg = strdup("param error, no has element name\n");
        return ;
    }
    if(!json_object_object_get_ex(obj, "exposuretime", &exposuretime)) {
        printf("param error, no has element name \n");
        *res_status = -1;
        *res_msg = strdup("param error, no has element name\n");
        return ;
    }
    if(!json_object_object_get_ex(obj, "exposuremode", &exposuremode)) {
        printf("param error, no has element name \n");
        *res_status = -1;
        *res_msg = strdup("param error, no has element name\n");
        return ;
    }
    if(!json_object_object_get_ex(obj, "irismode", &irismode)) {
        printf("param error, no has element name \n");
        *res_status = -1;
        *res_msg = strdup("param error, no has element name\n");
        return ;
    }
    if(!json_object_object_get_ex(obj, "irislevel", &irislevel)) {
        printf("param error, no has element name \n");
        *res_status = -1;
        *res_msg = strdup("param error, no has element name\n");
        return ;
    }

    const char *_brightness = json_object_get_string(brightness);
    const char *_contrast = json_object_get_string(contrast);
    const char *_colorsaturation = json_object_get_string(colorsaturation);
    const char *_sharpness = json_object_get_string(sharpness);
    const char *_exposuretime = json_object_get_string(exposuretime);
    const char *_exposuremode = json_object_get_string(exposuremode);
    const char *_irismode = json_object_get_string(irismode);
    const char *_irislevel = json_object_get_string(irislevel);

    int Brightness = atoi(_brightness);
    int Contrast = atoi(_contrast);
    int ColorSaturation = atoi(_colorsaturation);
    int Sharpness = atoi(_sharpness);
    int Exposuretime = atoi(_exposuretime);
    int ExposureMode = atoi(_exposuremode);
    int IrisMode = atoi(_irismode);
    int IrisLevel = atoi(_irislevel);


    MEDIAPIPE_SET_PROPERTY(ret, mp, "src", "brightness", Brightness, NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, "src", "contrast", Contrast, NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, "src", "saturation", ColorSaturation, NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, "src", "sharpness", Sharpness, NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, "src", "exposure-time", Exposuretime, NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, "src", "exp-priority", ExposureMode, NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, "src", "iris-mode", IrisMode, NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, "src", "iris-level", IrisLevel, NULL);

    *res_status = 1;
    *res_msg = strdup("_set_image_config success\n");
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
            printf("Can't get the required channel!!!\n");
            return ;
        }

    } else {
        if(_read_write_file(file_path, elem_format, enc_name, 0)) {
            printf("Can't get the required channel!!!\n");
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


    *res_status = 1;
    *res_msg = strdup("set mediapipe property success\n");

}

static void _get_image_config(struct json_object *obj, int *res_status,
                       gchar **res_msg, gpointer data)
{
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

    *res_status = 1;
    *res_msg = strdup("set mediapipe property success\n");
}

static void _get_range(struct json_object *obj, int *res_status, gchar **res_msg,
                gpointer data)
{

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
    param = g_object_class_find_property(G_OBJECT_GET_CLASS(element),
                                         "brightness");
    if(param != NULL) {
        pint = G_PARAM_SPEC_INT(param);
        if(pint != NULL) {
            brightness_min = pint->minimum;
            brightness_max = pint->maximum;
            pint = NULL;
        }
        param = NULL;
    }
    param = g_object_class_find_property(G_OBJECT_GET_CLASS(element),
                                         "saturation");
    if(param != NULL) {
        pint = G_PARAM_SPEC_INT(param);
        if(pint != NULL) {
            colorsaturation_min = pint->minimum;
            colorsaturation_max = pint->maximum;
            pint = NULL;
        }
        param = NULL;
    }
    param = g_object_class_find_property(G_OBJECT_GET_CLASS(element), "contrast");
    if(param != NULL) {
        pint = G_PARAM_SPEC_INT(param);
        if(pint != NULL) {
            contrast_min = pint->minimum;
            contrast_max = pint->maximum;
            pint = NULL;
        }
        param = NULL;
    }
    param = g_object_class_find_property(G_OBJECT_GET_CLASS(element),
                                         "sharpness");
    if(param != NULL) {
        pint = G_PARAM_SPEC_INT(param);
        if(pint != NULL) {
            sharpness_min = pint->minimum;
            sharpness_max = pint->maximum;
            pint = NULL;
        }
        param = NULL;
    }
    param = g_object_class_find_property(G_OBJECT_GET_CLASS(element),
                                         "exposure-time");
    if(param != NULL) {
        pint = G_PARAM_SPEC_INT(param);
        if(pint != NULL) {
            exposuretime_min = pint->minimum;
            exposuretime_max = pint->maximum;
            pint = NULL;
        }
        param = NULL;
    }
    param = g_object_class_find_property(G_OBJECT_GET_CLASS(element),
                                         "iris-level");
    if(param != NULL) {
        pint = G_PARAM_SPEC_INT(param);
        if(pint != NULL) {
            iris_min = pint->minimum;
            iris_max = pint->maximum;
            pint = NULL;
        }
        param = NULL;
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


    *res_status = 1;
    *res_msg = strdup("set mediapipe property success\n");
}


static void _get_stream_uri(struct json_object *obj, int *res_status, gchar **res_msg,
                     gpointer data)
{
    char streamuri[20] = {'\0'};
    sprintf(streamuri, "test%d", get_enc_num(enc_name, "c"));
    json_object_object_add(obj, "streamuri", json_object_new_string(streamuri));

    *res_status = 1;
    *res_msg = strdup("get streamuri success\n");
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
}


static void _handler(GIOChannel *gio, char *operation, struct json_object *param,
              int *res_status, gchar **res_msg, gpointer data)
{
    func f = get_func(operation);
    f(param, res_status, res_msg, data);
}

static int parse(char *s, char **operation, struct json_object **param)
{
    char ope[1024];
    strcpy(ope, s);
    const char *needle = "@";
    char *p = strstr(ope, needle);

    if(p == NULL) {
        return -1;
    }

    p[0] = '\0';
    p = p + strlen(needle);

    *operation = ope;
    *param = json_tokener_parse(p);
    if(*param == NULL) {
        return 0;
    }

    return 1;

}

static void handle(GIOChannel *gio, char *s, int *res_status, gchar **res_msg,
            gpointer data, char *res)
{
    //    char *param;
    char *operation;
    struct json_object *param;

    if(parse(s, &operation, &param) == 1) {
        strcpy(res, operation);
        strcat(res, "@");
        _handler(gio, operation, param, res_status, res_msg, data);
        strcat(res, json_object_to_json_string(param));
    } else {
        *res_status = -1;
        *res_msg = strdup("parse param error\n");
    }
}

static gboolean gio_client_read_in_hanlder(GIOChannel *gio, GIOCondition condition,
                                    gpointer data)
{
    GIOStatus ret;
    GError *err = NULL;
    gchar *msg = NULL;
    gsize len;
    char res[256] = {'\0'};

    gchar *res_msg = NULL;
    int res_status;

    if(condition & G_IO_HUP) {
        printf("client Read end of pipe died!\n");
        return TRUE;
    }

    ret = g_io_channel_read_line(gio, &msg, &len, NULL, &err);

    if(len == 0) {
        return FALSE;
    }
    char _msg[512] = {'\0'};
    strcpy(_msg, msg);
    g_free(msg);

    if(ret == G_IO_STATUS_ERROR) {
        printf("Error reading: %s\n", err->message);
        g_io_channel_shutdown(gio, TRUE, &err);
        return FALSE;
    } else if(ret == G_IO_STATUS_EOF) {
        g_io_channel_shutdown(gio, TRUE, &err);
        return FALSE;
    } else {
        handle(gio, _msg, &res_status, &res_msg, data, res);

        GIOStatus ret;
        GError *err = NULL;
        gsize len;

        char ret_msg[1024];
        if(res_status != 1) {
            strcpy(ret_msg, "error@");
            if(res_msg != NULL)
                 strcat(ret_msg, res_msg);
        } else {
            if(strstr(res, "get_videoenc_config") != NULL
               || strstr(res, "get_image_config") || strstr(res, "get_range")
               || strstr(res, "get_stream_uri")) {
                strcpy(ret_msg, res);
            } else {
                strcpy(ret_msg, "success@");
                if(res_msg != NULL)
                    strcat(ret_msg, res_msg);
            }
        }

        int insock = g_io_channel_unix_get_fd(gio);
        send(insock, ret_msg, strlen(ret_msg), 0);

        g_free(res_msg);
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
        printf("Unexpected Broken pipe error on socket_fd\n");
        close(socket_fd);
        return FALSE;
    }


    client_socket = accept(socket_fd, NULL, NULL);

    if(client_socket < 0) {
        printf("ERROR CLIENT_SOCKET VALUE!!!!!!!!!!!!!!!!!!!!\n");
        return FALSE;
    }

    client_channel = g_io_channel_unix_new(client_socket);
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

static void destroy_context(Context **ctx)
{
    g_free(*ctx);
}

static void onvif_server_start(mediapipe_t *mp)
{
    Context *ctx = create_context(mp);

    GIOChannel *gio_socket_channel;
    gint server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_sockaddr;
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port = htons(SERVER_PORT);
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);


    if(bind(server_sockfd, (struct sockaddr *) &server_sockaddr,
            sizeof(server_sockaddr)) == -1) {
        perror("bind error");
        exit(1);
    }

    if(listen(server_sockfd, 20) == -1) {
        perror("Communication listen error");
        exit(1);
    }
    printf("Communication server listen at: %d\n", SERVER_PORT);

    gio_socket_channel = g_io_channel_unix_new(server_sockfd);
    if(!gio_socket_channel) {
        g_error("Cannot create new Communication server GIOChannel!\n");
        exit(1);
    }
    GIOCondition cond = G_IO_IN;
    g_io_add_watch(gio_socket_channel, cond, (GIOFunc) gio_client_in_handle, ctx);
    g_io_channel_unref(gio_socket_channel);
    onvif_ctx = ctx;
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
    destroy_context(&onvif_ctx);
}
