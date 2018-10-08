#include "mediapipe_com.h"

static int mediapipe_set_key_frame(mediapipe_t *mp, const gchar *element_name);

static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata);

static mp_command_t  mp_encoder_commands[] = {
    {
        mp_string("encoder"),
        MP_MAIN_CONF,
        NULL,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_core_module_t  mp_encoder_module_ctx = {
    mp_string("encoder"),
    NULL,
    NULL
};

mp_module_t  mp_encoder_module = {
    MP_MODULE_V1,
    &mp_encoder_module_ctx,                /* module context */
    mp_encoder_commands,                   /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    keyshot_process,                    /* keyshot_process*/
    NULL,                               /* message_process */
    NULL,                      /* init_callback */
    NULL,                               /* netcommand_process */
    NULL,                               /* exit master */
    MP_MODULE_V1_PADDING
};

static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata)
{
    if (userdata == NULL) {
        return MP_ERROR;
    }

    char *key = (char *) userdata;

    if (key[0]=='?') {
//        printf(" ===== 'r' : switch resolution between 1080p and 720o    =====\n");
//        printf(" ===== 's' : switch rotation dynamically                 =====\n");
        printf(" ===== 'b' : switch bitrate between 5120000 and 10240000 =====\n");
        printf(" ===== 'f' : switch framerate between 30fps and 10fps    =====\n");
        printf(" ===== 'v' : switch br between cbr and vbr               =====\n");
//        printf(" ===== 'p' : switch qp between 10 and 40 when using CQP  =====\n");
        printf(" ===== 'o' : switch gop between 128 and 90               =====\n");
        return MP_IGNORE;
    }

    const char  *pro_keys="rsbfvop";
    gboolean find = FALSE;
    guint ret=0;
    GstCaps *caps;

    for (int i=0; i<strlen(pro_keys); i++) {
        if (key[0] ==pro_keys[i]) {
            find = TRUE;
            break;
        }
    }

    if (!find) {
        return MP_IGNORE;
    }

    if (key[0] == 'r') {
        static int size = 0;

        if (0 == size) {
            caps = gst_caps_from_string("video/x-raw(memory:MFXSurface),format=NV12,width=1280,height=720");
            MEDIAPIPE_SET_PROPERTY(ret, mp, "scale0_mfx_caps", "caps", caps, NULL);
            gst_caps_unref(caps);
            size = 1;
        } else {
            caps = gst_caps_from_string("video/x-raw(memory:MFXSurface),format=NV12,width=1920,height=1080");
            MEDIAPIPE_SET_PROPERTY(ret, mp, "scale0_mfx_caps", "caps", caps, NULL);
            gst_caps_unref(caps);
            size = 0;
        }
    } else if (key[0] == 's') {
        int rotate_method = 0;
        MEDIAPIPE_GET_PROPERTY(ret, mp, "rotate", "method", &rotate_method, NULL);
        rotate_method = (rotate_method + 1) % 6;
        MEDIAPIPE_SET_PROPERTY(ret, mp, "rotate", "method", rotate_method , NULL);

        if (1 == rotate_method || 3 == rotate_method) {
            caps = gst_caps_from_string("video/x-raw(memory:MFXSurface),"
                                        " format=NV12,width=1080,height=1920");
        } else {
            caps = gst_caps_from_string("video/x-raw(memory:MFXSurface),"
                                        " format=NV12,width=1920,height=1080");
        }

        MEDIAPIPE_SET_PROPERTY(ret, mp, "rotate_mfx_caps", "caps", caps, NULL);
        gst_caps_unref(caps);
    } else if (key[0] == 'b') {
        static int bps = 5120;
        bps = (bps <= 5120 ? 10240 : 5120);
        MEDIAPIPE_SET_PROPERTY(ret, mp, "enc0", "bitrate", bps, NULL);
    } else if (key[0] == 'f') {
        static unsigned int fps = 30;

        if (fps == 30) {
            fps = 10;
            caps = gst_caps_from_string("video/x-raw,framerate=10/1");
        } else {
            fps = 30;
            caps = gst_caps_from_string("video/x-raw,framerate=30/1");
        }

        MEDIAPIPE_SET_PROPERTY(ret, mp, "videorate_caps", "caps", caps, NULL);
        MEDIAPIPE_SET_PROPERTY(ret, mp, "enc0", "fps", fps, NULL);
        MEDIAPIPE_SET_PROPERTY(ret, mp, "enc1", "fps", fps, NULL);
        MEDIAPIPE_SET_PROPERTY(ret, mp, "enc2", "fps", fps, NULL);
        gst_caps_unref(caps);
    } else if (key[0] == 'v') {
        int br_mode = 0;
        MEDIAPIPE_GET_PROPERTY(ret, mp, "enc0", "rate-control", &br_mode, NULL);

        if (br_mode != 0) {
            if (4 == br_mode) {
                MEDIAPIPE_SET_PROPERTY(ret, mp, "enc0", "rate-control", 2, NULL);
                printf("INFO:VBR switches to CBR !!\n");
            } else if (2 == br_mode) {
                MEDIAPIPE_SET_PROPERTY(ret, mp, "enc0", "rate-control", 4, NULL);
                printf("INFO:CBR switches to VBR !!\n");
            } else {
                printf("ERROR:Features only apply to CBR/VBR !!\n");
            }
        }
    } else if (key[0] == 'p') {
        static unsigned int qp = 10;
        int br_mode = 0;
        MEDIAPIPE_GET_PROPERTY(ret, mp, "enc0", "rate-control", &br_mode, NULL);

        if (br_mode == 3) { //under CQP mode
            qp = (qp == 40) ? 10 : 40;
            MEDIAPIPE_SET_PROPERTY(ret, mp, "enc0", "quantizer", qp, NULL);
            MEDIAPIPE_SET_PROPERTY(ret, mp, "enc0", "qpi-offset", 0, NULL);
            MEDIAPIPE_SET_PROPERTY(ret, mp, "enc0", "qpp-offset", 0, NULL);
            MEDIAPIPE_SET_PROPERTY(ret, mp, "enc0", "qpb-offset", 0, NULL);
        }
    } else if (key[0] == 'o') {
        static unsigned int gop = 128;
        gop = (gop == 128) ? 90 : 128;
        MEDIAPIPE_SET_PROPERTY(ret, mp, "enc0", "keyframe-period", gop, NULL);
    } else if (key[0] == 'h') {
        MEDIAPIPE_SET_PROPERTY(ret, mp, "src", "custom-aic-param",
                               "1,1,1,1,1,1,1,1,20,", NULL);
        MEDIAPIPE_SET_PROPERTY(ret, mp, "src", "scene-mode", 7, NULL);
    }

    if (!ret) {
        return MP_OK;
    } else {
        return MP_ERROR;
    }
}

