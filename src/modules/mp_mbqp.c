/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mediapipe_com.h"
#include "mp_mbqpmeta.h"

typedef struct {
    GList *mbqp_list;
} mbqp_ctx_t;


static GstVideoMBQP *
create_mbqp_meta(struct json_object *object);

static GValueArray *
gst_video_mbqp_process(GList *list, GstCaps *caps, guchar  quantizer);

static void
mediapipe_add_mbqp_list(mediapipe_t *mp, GList *list);

static GList *
json_parse_mbqp_info(struct json_object *root);

static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata);

static void exit_master(void);


static char *
mp_mbqp_block(mediapipe_t *mp, mp_command_t *cmd);

static int MB_SIZE = 0;

static mp_command_t  mp_mbqp_commands[] = {
    {
        mp_string("mbqp"),
        MP_MAIN_CONF,
        mp_mbqp_block,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_core_module_t  mp_mbqp_module_ctx = {
    mp_string("mbqp"),
    NULL,
    NULL
};

static mbqp_ctx_t  ctx = {
    NULL
};

mp_module_t  mp_mbqp_module = {
    MP_MODULE_V1,
    &mp_mbqp_module_ctx,                /* module context */
    mp_mbqp_commands,                   /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    keyshot_process,                    /* keyshot_process*/
    NULL,                               /* message_process */
    NULL,                               /* init_callback */
    NULL,                               /* netcommand_process */
    exit_master,                        /* exit master */
    MP_MODULE_V1_PADDING
};

static char *
mp_mbqp_block(mediapipe_t *mp, mp_command_t *cmd)
{
    ctx.mbqp_list = json_parse_mbqp_info(mp->config);
    return MP_CONF_OK;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis create mbqp meta from json object
 *
 * @Param object
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static GstVideoMBQP *
create_mbqp_meta(struct json_object *object)
{
    gint x, y, width, height, value;
    RETURN_VAL_IF_FAIL(json_get_int(object, "x", &x), NULL);
    RETURN_VAL_IF_FAIL(json_get_int(object, "y", &y), NULL);
    RETURN_VAL_IF_FAIL(json_get_int(object, "width", &width), NULL);
    RETURN_VAL_IF_FAIL(json_get_int(object, "height", &height), NULL);
    RETURN_VAL_IF_FAIL(json_get_int(object, "value", &value), NULL);
    RETURN_VAL_IF_FAIL(width > 0 && height > 0, NULL);
    return gst_video_mbqp_create(x, y, width, height, value);
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis create mbqp meta list from root json object
 *
 * @Param root
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static GList *
json_parse_mbqp_info(struct json_object *root)
{
    GList *list = NULL;
    struct json_object *parent, *object;
    RETURN_VAL_IF_FAIL(root != NULL, NULL);
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(root, "mbqp", &parent), NULL);
    guint num_mbqps = json_object_array_length(parent);

    for (guint i = 0; i < num_mbqps; ++i) {
        object = json_object_array_get_idx(parent, i);
        GstVideoMBQP *meta = create_mbqp_meta(object);

        if (meta) {
            list = g_list_append(list, meta);
        }
    }

    return list;
}

/**
    @brief  mbqp check List and mediapipe  then  added to process

    @param mp Pointer to mediapipe.
    @param list The list of MBQP area.
*/
static void
mediapipe_add_mbqp_list(mediapipe_t *mp, GList *list)
{
    g_assert(mp);
    int ret = 0;
    guchar  quantizer;
    GstCaps *caps = NULL;
    GValueArray *array = NULL;
    MEDIAPIPE_GET_PROPERTY(ret, mp, "enc0", "mbqp", &array, NULL);

    if (ret == 0 && NULL == array) {
        MEDIAPIPE_GET_PROPERTY(ret, mp, "scale0_mfx_caps", "caps", &caps, NULL);

        if (ret == 0) {
            MB_SIZE = 16;
        }

        MEDIAPIPE_GET_PROPERTY(ret, mp, "enc0", "quantizer", &quantizer, NULL);

        if (ret == 0) {
            array = gst_video_mbqp_process(list, caps, quantizer);
        }

        MEDIAPIPE_SET_PROPERTY(ret, mp, "enc0", "mbqp", array, NULL);
        g_value_array_free(array);
    }

    MEDIAPIPE_GET_PROPERTY(ret, mp, "enc2", "mbqp", &array, NULL);

    if (ret == 0 && NULL == array) {
        MEDIAPIPE_GET_PROPERTY(ret, mp, "scale2_mfx_caps", "caps", &caps, NULL);

        if (ret == 0) {
            MB_SIZE = 32;
        }

        MEDIAPIPE_GET_PROPERTY(ret, mp, "enc2", "quantizer", &quantizer, NULL);

        if (ret == 0) {
            array = gst_video_mbqp_process(list, caps, quantizer);
        }

        MEDIAPIPE_SET_PROPERTY(ret, mp, "enc2", "mbqp", array, NULL);
        g_value_array_free(array);
    }

    return;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis convert meta list to the encoder property format
 *
 * @Param list
 * @Param caps
 * @Param quantizer
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static GValueArray *
gst_video_mbqp_process(GList *list, GstCaps *caps, guchar  quantizer)
{
    gint16 index_x , index_xl , index_y , index_yl , index_w, index_h, sum = 0,
                                                                       count = 0;
    gint  width, height;
    GList *iterator = NULL;
    GstVideoMBQP *Gmbqp = NULL;
    GValueArray *array = NULL;
    get_resolution_from_caps(caps, &width, &height);
    index_w = ALIGN_MB(width, MB_SIZE) / MB_SIZE;
    index_h = ALIGN_MB(height, MB_SIZE) / MB_SIZE;
    sum = index_w * index_h;
    array = g_value_array_new(1);
    GValue v = { 0, };
    g_value_init(&v, G_TYPE_UCHAR);

    for (gint16 i = 0; i < sum; i++) {
        g_value_set_uchar(&v, quantizer);
        g_value_array_append(array, &v);
    }

    if (NULL == list) {
        g_print("ERROR ### Can not find config for MBQP!!! ");
        return array;
    }

    for (iterator = list; iterator; iterator = iterator->next) {
        Gmbqp = (GstVideoMBQP *)iterator->data;
        guchar num = (guchar)quantizer + (guchar)Gmbqp->value;

        if ((Gmbqp->x + Gmbqp->w) <= width && (Gmbqp->y + Gmbqp->h) <= height
            && num > 0) {
            index_x = ALIGN_MB(Gmbqp->x, MB_SIZE) / MB_SIZE;

            if (Gmbqp->x + Gmbqp->w >= width) {
                index_x = index_x - 1;
            }

            index_y = ALIGN_MB(Gmbqp->y, MB_SIZE) / MB_SIZE;

            if (Gmbqp->y + Gmbqp->h >= height) {
                index_y = index_y - 1;
            }

            index_xl = ALIGN_MB(Gmbqp->w, MB_SIZE) / MB_SIZE;
            index_yl = ALIGN_MB(Gmbqp->h, MB_SIZE) / MB_SIZE;
            g_print("QP = %d \n", num);

            for (gint16  i = 0; i < index_xl ; i++) {
                for (gint16  y = 0; y < index_yl ; y++) {
                    g_value_set_uchar(g_value_array_get_nth(array,
                                                            index_y * index_w + index_x + i + y * index_w), num);
                }
            }

            if (8 <= count++) {
                continue;
            }
        }
    }

    return array;
}

static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata)
{
    mp_int_t ret = MP_OK;

    if (userdata == NULL) {
        return MP_ERROR;
    }

    char *key = (char *) userdata;

    if (key[0]=='?') {
        printf(" ===== 'm' : mbqp example                                =====\n");
        return MP_IGNORE;
    }

    if (key[0] != 'm') {
        return MP_IGNORE;
    }

    static gboolean enable_mbqp_bool = TRUE;

    if (enable_mbqp_bool) {
        mediapipe_add_mbqp_list(mp, ctx.mbqp_list);
        MEDIAPIPE_SET_PROPERTY(ret, mp, "enc0", "enableMBQP", TRUE, NULL);
        MEDIAPIPE_SET_PROPERTY(ret, mp, "enc2", "enableMBQP", TRUE, NULL);
    } else {
        MEDIAPIPE_SET_PROPERTY(ret, mp, "enc0", "enableMBQP", FALSE, NULL);
        MEDIAPIPE_SET_PROPERTY(ret, mp, "enc2", "enableMBQP", FALSE, NULL);
    }

    printf("turn mbqp %s\n", enable_mbqp_bool ? "on" : "off");
    enable_mbqp_bool = ! enable_mbqp_bool;
    return ret;
}

static void exit_master(void)
{
    g_list_free_full(ctx.mbqp_list, (GDestroyNotify)g_free);
}




