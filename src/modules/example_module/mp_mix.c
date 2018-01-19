#include "mediapipe_com.h"
#include "font.h"

#include "gstocl/oclcommon.h"



#define ONE_BILLON_NANO_SECONDS 1000000000

typedef enum {
    DEFAULT_TRACK_ID = 0,
    WF_FD_TRACK_ID,
    WF_MD_TRACK_ID,
    WF_PD_TRACK_ID,
    OSD_CLOCK_TRACK_ID,
    OSD_CPU_RATE_TRACK_ID,
    OSD_GPU_RATE_TRACK_ID,
    OSD_FLOW_TEXT_TRACK_ID
} oclmix_tracking_id_t;

typedef struct {
    GstClockTime    t;
    struct timespec ts;
} time_stamp_t;

static time_stamp_t osd_timestamp_ctx;


static gpointer
create_logo_param(struct json_object *object);

static gpointer
create_mask_param(struct json_object *object);

static gpointer
create_mosaic_param(struct json_object *object);

static gpointer
create_osd_param(struct json_object *object);

static gpointer
create_wireframe_param(struct json_object *object);

static gchar *
get_osd_timestamp_string();

static gboolean
init_osd_rgba_data(OclMixParam *param, guint num, struct json_object *object);

static gboolean
json_setup_oclmix_element(mediapipe_t *mp, const gchar *elem_name);


static gboolean
osd_clock_update(OclMixParam *param, GstClockTime pts);

static gboolean
osd_cpu_rate_update(OclMixParam *param);

static gboolean
osd_flow_text_update(OclMixParam *param);

static gboolean
osd_gpu_rate_update(OclMixParam *param);

static void
osd_meta_process(gpointer data, gpointer user_data);

static void
osd_timestamp_context_prepare(GstElement *pipeline);

static GstPadProbeReturn
osd_timestamp_ctx_init_callback(GstPad *pad, GstPadProbeInfo *info,
                                gpointer user_data);

static gboolean
update_osd_meta_list(GList *osd_list, GstClockTime pts);

static char *
mp_mix_block(mediapipe_t *mp, mp_command_t *cmd);

static void
exit_master(void);

static gpointer g_font_library[] = { (gpointer) font, (gpointer) time_font };

static mp_command_t  mp_mix_commands[] = {
    {
        mp_string("mix"),
        MP_MAIN_CONF,
        mp_mix_block,
        0,
        0,
        NULL
    },
    mp_null_command
};


static mp_core_module_t  mp_mix_module_ctx = {
    mp_string("mix"),
    NULL,
    NULL
};


mp_module_t  mp_mix_module = {
    MP_MODULE_V1,
    &mp_mix_module_ctx,                /* module context */
    mp_mix_commands,                   /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                              /* init master */
    NULL,                              /* init module */
    NULL,                              /* keyshot_process*/
    NULL,                              /* message_process */
    NULL,                              /* init_callback */
    NULL,                              /* netcommand_process */
    exit_master,                       /* exit master */
    MP_MODULE_V1_PADDING
};


static gpointer
create_logo_param(struct json_object *object)
{
    const char *filename;
    guint x, y, width, height;
    RETURN_VAL_IF_FAIL(json_get_string(object, "filename", &filename), NULL);
    char *rgba_data = read_file(filename);
    RETURN_VAL_IF_FAIL(rgba_data != NULL, NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "x", &x), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "y", &y), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "width", &width), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "height", &height), NULL);
    RETURN_VAL_IF_FAIL(width != 0 && height != 0, NULL);
    OclMixParam *param = ocl_mix_meta_new();
    param->rect.x = x;
    param->rect.y = y;
    param->rect.width = width;
    param->rect.height = height;
    param->raw_data = rgba_data;
    param->flag = OCL_MIX_LOGO;
    return (gpointer) param;
    return NULL;
}

static gpointer
create_mask_param(struct json_object *object)
{
    guint32 color;
    guint x, y, width, height;
    RETURN_VAL_IF_FAIL(json_get_uint(object, "x", &x), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "y", &y), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "width", &width), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "height", &height), NULL);
    RETURN_VAL_IF_FAIL(json_get_rgba(object, "color_rgba", &color), NULL);
    RETURN_VAL_IF_FAIL(width != 0 && height != 0, NULL);
    OclMixParam *param = ocl_mix_meta_new();
    param->rect.x = x;
    param->rect.y = y;
    param->rect.width = width;
    param->rect.height = height;
    param->raw_data = fakebuff_create(color, width, height);
    param->flag = OCL_MIX_MASK;
    return (gpointer) param;
    return NULL;
}


static gchar *
get_osd_timestamp_string()
{
    struct timespec ts;
    static gchar clock_string[32];
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm *ct = localtime(&ts.tv_sec);
    g_assert(ct != NULL);
    snprintf(clock_string, 32, "%04d-%02d-%02d %02d:%02d:%02d", ct->tm_year + 1900,
             ct->tm_mon + 1, ct->tm_mday, ct->tm_hour, ct->tm_min, ct->tm_sec);
    return clock_string;
}

static gboolean
init_osd_rgba_data(OclMixParam *param, guint num, struct json_object *object)
{
    const char *text = "Null Text";
    const char *font_desc = "Sans 32";

    if (num) {
        param->raw_data = (gchar *) &g_font_library[0];
        param->array = g_array_sized_new(FALSE, TRUE, sizeof(guint), num);

        for (guint index = 0; index < num; ++index) {
            g_array_append_val(param->array, index);
        }

        return TRUE;
    }

    if (param->track_id == (guint32) OSD_CLOCK_TRACK_ID) {
        text = get_osd_timestamp_string();
    } else {
        json_get_string(object, "text", &text);
    }

    guint num_characters = strlen(text);
    json_get_uint(object, "max-stride", &num_characters);
    json_get_string(object, "font", &font_desc);
    cairo_render_get_suggest_font_size(font_desc, &param->rect.width,
                                       &param->rect.height);
    param->rect.width = param->rect.width * num_characters;
    cairo_render_t *render = cairo_render_create(param->rect.width, param->rect.height,
                          font_desc);
    g_caior_render_list = g_list_append(g_caior_render_list, render);
    param->user_data[0] = (gpointer) render;
    param->raw_data = cairo_render_get_rgba_data(render, text);
    return TRUE;
}

static gpointer
create_osd_param(struct json_object *object)
{
    guint x, y, num = 0;
    const char *name = NULL;
    RETURN_VAL_IF_FAIL(json_get_uint(object, "x", &x), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "y", &y), NULL);
    json_get_uint(object, "num", &num);
    json_get_string(object, "name", &name);
    RETURN_VAL_IF_FAIL(num != 0 || name != NULL, NULL);
    OclMixParam *param = ocl_mix_meta_new();
    param->rect.x = x;
    param->rect.y = y;
    param->rect.width = FONT_BLOCK_SIZE;
    param->rect.height = FONT_BLOCK_SIZE;
    param->flag = OCL_MIX_OSD;

    if (name == NULL) {
        param->track_id = (guint32) DEFAULT_TRACK_ID;
    } else if (!strcmp(name, "clock")) {
        param->track_id = (guint32) OSD_CLOCK_TRACK_ID;
    } else if (!strcmp(name, "cpu")) {
        param->track_id = (guint32) OSD_CPU_RATE_TRACK_ID;
    } else if (!strcmp(name, "gpu")) {
        param->track_id = (guint32) OSD_GPU_RATE_TRACK_ID;
    } else if (!strcmp(name, "flow_text")) {
        param->track_id = (guint32) OSD_FLOW_TEXT_TRACK_ID;
    }

    if (!init_osd_rgba_data(param, num, object)) {
        g_free(param);
        return NULL;
    }

    return (gpointer) param;
    return NULL;
}

static gpointer
create_mosaic_param(struct json_object *object)
{
    guint x, y, width, height;
    RETURN_VAL_IF_FAIL(json_get_uint(object, "x", &x), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "y", &y), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "width", &width), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "height", &height), NULL);
    RETURN_VAL_IF_FAIL(width != 0 && height != 0, NULL);
    OclMixParam *param = ocl_mix_meta_new();
    param->rect.x = x;
    param->rect.y = y;
    param->rect.width = width;
    param->rect.height = height;
    param->flag = OCL_MIX_MOSAIC;
    return (gpointer) param;
    return NULL;
}

static gpointer
create_wireframe_param(struct json_object *object)
{
    guint x, y, width, height;
    RETURN_VAL_IF_FAIL(json_get_uint(object, "x", &x), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "y", &y), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "width", &width), NULL);
    RETURN_VAL_IF_FAIL(json_get_uint(object, "height", &height), NULL);
    RETURN_VAL_IF_FAIL(width != 0 && height != 0, NULL);
    OclMixParam *param = ocl_mix_meta_new();
    param->rect.x = x;
    param->rect.y = y;
    param->rect.width = width;
    param->rect.height = height;
    param->flag = OCL_MIX_WIREFRAME;
    return (gpointer) param;
    return NULL;
}

static void
osd_meta_process(gpointer data, gpointer user_data)
{
    OclMixParam *param = (OclMixParam *) data;
    GstClockTime pts = *((GstClockTime *) user_data);
    oclmix_tracking_id_t track_id = (oclmix_tracking_id_t) param->track_id;

    switch (track_id) {
    case OSD_CLOCK_TRACK_ID:
        osd_clock_update(param, pts);
        break;

    case OSD_CPU_RATE_TRACK_ID:
        osd_cpu_rate_update(param);
        break;

    case OSD_GPU_RATE_TRACK_ID:
        osd_gpu_rate_update(param);
        break;

    case OSD_FLOW_TEXT_TRACK_ID:
        osd_flow_text_update(param);
        break;

    default:
        break;
    }
}

static gboolean
update_osd_meta_list(GList *osd_list, GstClockTime pts)
{
    RETURN_VAL_IF_FAIL(osd_list != NULL, FALSE);
    g_list_foreach(osd_list, osd_meta_process, (gpointer) &pts);
    return TRUE;
}

static gboolean
json_setup_oclmix_element(mediapipe_t *mp, const gchar *elem_name)
{
    GList *logo_list, *mask_list, *osd_list, *mosaic_list, *wireframe_list;
    struct json_object *mix_root;
    RETURN_VAL_IF_FAIL(mp != NULL, FALSE);
    RETURN_VAL_IF_FAIL(mp->config != NULL, FALSE);
    RETURN_VAL_IF_FAIL(elem_name != NULL, FALSE);
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(mp->config, "mix",
                   &mix_root),FALSE);
    GstElement *element = gst_bin_get_by_name(GST_BIN(mp->pipeline), elem_name);

    if (!element) {
        LOG_WARNING("Add callback failed, can not find element \"%s\"", elem_name);
        return FALSE;
    }

    logo_list = json_parse_config(mix_root, "logo", create_logo_param);

    if (logo_list != NULL) {
        ocl_mix_meta_list_append(element, logo_list, OCL_MIX_LOGO);
    }

    mask_list = json_parse_config(mix_root, "mask", create_mask_param);

    if (mask_list != NULL) {
        ocl_mix_meta_list_append(element, mask_list, OCL_MIX_MASK);
    }

    osd_list = json_parse_config(mix_root, "osd", create_osd_param);

    if (osd_list != NULL) {
        ocl_mix_meta_list_append(element, osd_list, OCL_MIX_OSD);
    }

    mosaic_list = json_parse_config(mix_root, "mosaic", create_mosaic_param);

    if (mosaic_list != NULL) {
        ocl_mix_meta_list_append(element, mosaic_list, OCL_MIX_MOSAIC);
    }

    wireframe_list = json_parse_config(mix_root, "wf", create_wireframe_param);

    if (wireframe_list != NULL) {
        ocl_mix_meta_list_append(element, wireframe_list, OCL_MIX_WIREFRAME);
    }

    osd_timestamp_context_prepare(mp->pipeline);
    ocl_mix_set_osd_meta_update_callback(element, update_osd_meta_list);
    gst_object_unref(element);
    return TRUE;
}


static GstPadProbeReturn
osd_timestamp_ctx_init_callback(GstPad *pad, GstPadProbeInfo *info,
                                gpointer user_data)
{
    osd_timestamp_ctx.t = GST_BUFFER_PTS(GST_PAD_PROBE_INFO_BUFFER(info));
    clock_gettime(CLOCK_REALTIME, &osd_timestamp_ctx.ts);
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    return GST_PAD_PROBE_REMOVE;
}

static void
osd_timestamp_context_prepare(GstElement *pipeline)
{
    GstElement *camera = gst_bin_get_by_name(GST_BIN(pipeline), "src");
    GstPad *pad = gst_element_get_static_pad(camera, "src");

    if (pad) {
        gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                          osd_timestamp_ctx_init_callback, NULL, NULL);
        gst_object_unref(pad);
    }

    gst_object_unref(camera);
}

static gboolean
osd_clock_update(OclMixParam *param, GstClockTime pts)
{
    RETURN_VAL_IF_FAIL(param != NULL, FALSE);
    RETURN_VAL_IF_FAIL(param->user_data[0] != NULL, FALSE);
    GstClockTimeDiff diff = pts - osd_timestamp_ctx.t;
    RETURN_VAL_IF_FAIL(diff > 0, FALSE);
    struct timespec delta;
    GST_TIME_TO_TIMESPEC((GstClockTime) diff, delta);

    if (!delta.tv_sec
        && osd_timestamp_ctx.ts.tv_nsec + delta.tv_nsec < ONE_BILLON_NANO_SECONDS) {
        return FALSE;
    }

    osd_timestamp_ctx.t = pts;
    osd_timestamp_ctx.ts.tv_sec  += delta.tv_sec;
    osd_timestamp_ctx.ts.tv_nsec += delta.tv_nsec;

    if (osd_timestamp_ctx.ts.tv_nsec / ONE_BILLON_NANO_SECONDS) {
        osd_timestamp_ctx.ts.tv_sec  += osd_timestamp_ctx.ts.tv_nsec /
                                        ONE_BILLON_NANO_SECONDS;
        osd_timestamp_ctx.ts.tv_nsec %= ONE_BILLON_NANO_SECONDS;
    }

    struct tm *ct = localtime(&osd_timestamp_ctx.ts.tv_sec);

    g_assert(ct != NULL);

    gchar clock_string[32];

    snprintf(clock_string, 32, "%04d-%02d-%02d %02d:%02d:%02d", ct->tm_year + 1900,
             ct->tm_mon + 1, ct->tm_mday, ct->tm_hour, ct->tm_min, ct->tm_sec);

    cairo_render_t *render = (cairo_render_t *) param->user_data[0];

    param->raw_data = cairo_render_get_rgba_data(render, clock_string);

    param->osd_update = TRUE;

    return TRUE;
}

static gboolean
osd_cpu_rate_update(OclMixParam *param)
{
    char cpu_message[32];
    RETURN_VAL_IF_FAIL(param != NULL, FALSE);
    RETURN_VAL_IF_FAIL(param->user_data[0] != NULL, FALSE);
    static guint cpu_rate = 0;

    if (++cpu_rate > 300) {
        cpu_rate = 0;
    }

    sprintf(cpu_message, "cpu: %02u", cpu_rate);
    cairo_render_t *render = (cairo_render_t *) param->user_data[0];
    param->raw_data = cairo_render_get_rgba_data(render, cpu_message);
    param->osd_update = TRUE;
    return TRUE;
}

static gboolean
osd_gpu_rate_update(OclMixParam *param)
{
    char gpu_message[32];
    RETURN_VAL_IF_FAIL(param != NULL, FALSE);
    RETURN_VAL_IF_FAIL(param->user_data[0] != NULL, FALSE);
    static guint gpu_rate = 0;

    if (++gpu_rate > 300) {
        gpu_rate = 0;
    }

    sprintf(gpu_message, "gpu: %02u", gpu_rate);
    cairo_render_t *render = (cairo_render_t *) param->user_data[0];
    param->raw_data = cairo_render_get_rgba_data(render, gpu_message);
    param->osd_update = TRUE;
    return TRUE;
}

static gboolean
osd_flow_text_update(OclMixParam *param)
{
    RETURN_VAL_IF_FAIL(param != NULL, FALSE);

    if (++param->rect.x > 1900) {
        param->rect.x = 0;
    }

    return TRUE;
}

static char *
mp_mix_block(mediapipe_t *mp, mp_command_t *cmd)
{
    json_setup_oclmix_element(mp, "mix");
    return MP_CONF_OK;
}

static void
exit_master(void)
{
}


