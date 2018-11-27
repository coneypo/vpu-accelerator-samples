#include "mediapipe_com.h"

typedef struct {
    GstClockTime pts;
    gsize        buf_size;
    slice_type_t    slice_type;
} encode_meta_t;

static mp_int_t
init_callback(mediapipe_t *mp);

static gboolean
enc2_src_callback(mediapipe_t *mp, GstBuffer *buf, guint8 *data, gsize size,
                  gpointer user_data);

static gboolean
enc0_src_callback(mediapipe_t *mp, GstBuffer *buf, guint8 *data, gsize size,
                  gpointer user_data);



static mp_command_t  mp_slice_commands[] = {
    {
        mp_string("slice"),
        MP_MAIN_CONF,
        NULL,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_core_module_t  mp_slice_module_ctx = {
    mp_string("slice"),
    NULL,
    NULL
};

mp_module_t  mp_slice_module = {
    MP_MODULE_V1,
    &mp_slice_module_ctx,                /* module context */
    mp_slice_commands,                   /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    NULL,                    /* keyshot_process*/
    NULL,                               /* message_process */
    NULL,                      /* init_callback */
    NULL,                               /* netcommand_process */
    NULL,                               /* exit master */
    MP_MODULE_V1_PADDING
};

static mp_int_t
init_callback(mediapipe_t *mp)
{
    mediapipe_set_user_callback(mp, "enc0", "src", enc0_src_callback, NULL);
    mediapipe_set_user_callback(mp, "enc2", "src", enc2_src_callback, NULL);
    return MP_OK;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis 264 video stream slice
 *
 * @Param mp
 * @Param buf
 * @Param data
 * @Param size
 * @Param user_data
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static gboolean
enc0_src_callback(mediapipe_t *mp, GstBuffer *buf, guint8 *data, gsize size,
                  gpointer user_data)
{
    GstMapInfo *info = NULL;

    if (!data || !size) {
        info = g_new0(GstMapInfo, 1);

        if (!gst_buffer_map(buf, info, GST_MAP_READ)) {
            g_print("gst_buffer_map() failed\n");
            g_free(info);
            return TRUE;
        }

        data = info->data;
        size = info->size;
    }

    encode_meta_t meta;
    meta.pts        = GST_BUFFER_PTS(buf);
    meta.buf_size   = gst_buffer_get_size(buf);
    meta.slice_type = h264_get_slice_type(data, size);

    if (meta.slice_type == SLICE_TYPE_FAIL) {
        g_print("Failed to parse slice type\n");
    }

    if (info) {
        gst_buffer_unmap(buf, info);
        g_free(info);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis 265 video strem slice
 *
 * @Param mp
 * @Param buf
 * @Param data
 * @Param size
 * @Param user_data
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static gboolean
enc2_src_callback(mediapipe_t *mp, GstBuffer *buf, guint8 *data, gsize size,
                  gpointer user_data)
{
    GstMapInfo *info = NULL;

    if (!data || !size) {
        info = g_new0(GstMapInfo, 1);

        if (!gst_buffer_map(buf, info, GST_MAP_READ)) {
            g_print("gst_buffer_map() failed\n");
            g_free(info);
            return TRUE;
        }

        data = info->data;
        size = info->size;
    }

    encode_meta_t meta;
    meta.pts        = GST_BUFFER_PTS(buf);
    meta.buf_size   = gst_buffer_get_size(buf);
    meta.slice_type = h265_get_slice_type(data, size);

    if (meta.slice_type == SLICE_TYPE_FAIL) {
        g_print("Failed to parse slice type\n");
    }

    if (info) {
        gst_buffer_unmap(buf, info);
        g_free(info);
    }

    return TRUE;
}


