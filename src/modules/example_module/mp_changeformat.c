#include "mediapipe_com.h"

typedef struct change_format_context_s change_format_context_t;

typedef gboolean (*format_change_extra_func_t)(gpointer user_data);

struct change_format_context_s {
    mediapipe_t           *mp;
    GstElement          *cur_element;
    GstElement          *up_element;
    GstElement          *down_element;
    gchar               *format_string;
    gchar               *enc_name;
    format_change_extra_func_t  extra_fun;
    gpointer             user_data;
};

static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata);

static GstPadProbeReturn
change_format_event_probe_cb(GstPad *pad, GstPadProbeInfo *info,
                             gpointer user_data);

static GstPadProbeReturn
change_format_pad_probe_cb(GstPad *pad, GstPadProbeInfo *info,
                           gpointer user_data);

static int mediapipe_change_format(mediapipe_t *mp, const gchar *cur_elem_name,
                                   const gchar *up_elem_name , const gchar *down_elem_name , const gchar *format,
                                   format_change_extra_func_t extra_fun , gpointer _user_data);
static gboolean
change_to265(gpointer user_data);

static gboolean
change_to264(gpointer user_data);

static mp_command_t  mp_changeformat_commands[] = {
    {
        mp_string("changeformat"),
        MP_MAIN_CONF,
        NULL,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_core_module_t  mp_changeformat_module_ctx = {
    mp_string("changeformat"),
    NULL,
    NULL
};


mp_module_t  mp_changeformat_module = {
    MP_MODULE_V1,
    &mp_changeformat_module_ctx,                /* module context */
    mp_changeformat_commands,                   /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    keyshot_process,                    /* keyshot_process*/
    NULL,                               /* message_process */
    NULL,                               /* init_callback */
    NULL,                               /* netcommand_process */
    NULL,                        /* exit master */
    MP_MODULE_V1_PADDING
};



static mp_int_t
keyshot_process(mediapipe_t *mp, void *userdata)
{
    mp_int_t ret = MP_OK;

    if (userdata == NULL) {
        return MP_ERROR;
    }

    char *key = (char *) userdata;

    if (key[0]=='?') {
        printf(" ===== 'a' : change format to 264                        =====\n");
        printf(" ===== 'A' : change format to 265                        =====\n");
        return MP_IGNORE;
    }

    if (key[0] != 'a' && key[0] != 'A') {
        return MP_IGNORE;
    }

    if (key[0] == 'a') {
        ret=mediapipe_change_format(mp, "enc3", "qfc", "enc3_caps",  "H264",
                                    change_to264,mp);
    } else if (key[0] == 'A') {
        ret=mediapipe_change_format(mp, "enc3", "qfc", "enc3_caps",  "H265",
                                    change_to265,mp);
    }

    return ret;
}

/* --------------------------------------------------------------------------*/
/**
    @Synopsis  change the format of encoding  to 264 or 265

    @Param mp                  Pointer to medipipe.
    @Param cur_elem_name       the current encoder  name
    @Param up_elem_name        the up element link to current encoder
    @Param down_elem_name      the down element link to current encoder
    @Param format              new format string  264 or 265
    @Param extra_fun           the extra funciton point for new caps and new file locaiton and more
    @Param _user_data          the userdata point for extra fun

    @Returns     success return 0
*/
/* ----------------------------------------------------------------------------*/
static int mediapipe_change_format(mediapipe_t *mp, const gchar *cur_elem_name,
                                   const gchar *up_elem_name , const gchar *down_elem_name , const gchar *format,
                                   format_change_extra_func_t extra_fun , gpointer _user_data)
{
    GstElement *up_element = gst_bin_get_by_name(GST_BIN((mp)->pipeline),
                             (up_elem_name));
    GstElement *down_element = gst_bin_get_by_name(GST_BIN((mp)->pipeline),
                               (down_elem_name));
    GstElement *cur_element = gst_bin_get_by_name(GST_BIN((mp)->pipeline),
                              (cur_elem_name));

    if (!up_element || !down_element || !cur_element) {
        return -1;
    }

    /* creat  context for change format */
    change_format_context_t *user_data = g_new0(change_format_context_t, 1);
    user_data->up_element = up_element;
    user_data->down_element = down_element;
    user_data->cur_element = cur_element;
    user_data->mp = mp;
    user_data->format_string = g_strdup(format);
    user_data->enc_name = g_strdup(cur_elem_name);
    user_data->extra_fun = extra_fun;
    user_data->user_data = _user_data;
    GstPad *blockpad = gst_element_get_static_pad(up_element, "src");
    gulong probe_id = gst_pad_add_probe(blockpad,
                                        GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                                        change_format_pad_probe_cb, user_data, NULL);
    g_object_unref(blockpad);
    return (probe_id ? 0 : -1);
}

/* --------------------------------------------------------------------------*/
/**
    @Synopsis it's a example of  extra function for change current encoder to 264
             format.  some extra  propertys  or some else  need to be changed
             after encoder changed , so do this here.

    @Param user_data

    @Returns
*/
/* ----------------------------------------------------------------------------*/
static gboolean
change_to264(gpointer user_data)
{
    mediapipe_t *mp = (mediapipe_t *) user_data;
    int ret = 1;
    GstElement *enc3_caps = gst_bin_get_by_name(GST_BIN((mp)->pipeline),
                            "enc3_caps");
    GstCaps *caps =
        gst_caps_from_string("video/x-h264,stream-format=byte-stream,profile=high");
    gst_element_set_state(enc3_caps, GST_STATE_NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, "enc3_caps", "caps", caps, NULL);
    gst_element_set_state(enc3_caps, GST_STATE_PLAYING);
    gst_caps_unref(caps);
    gst_object_unref(enc3_caps);
    GstElement *sink = gst_bin_get_by_name(GST_BIN((mp)->pipeline), "sink3");
    gst_element_set_state(sink, GST_STATE_NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, "sink3", "location", "forchange.264", NULL);
    gst_element_set_state(sink, GST_STATE_PLAYING);
    gst_object_unref(sink);
    return (ret == 0);
}

/* --------------------------------------------------------------------------*/
/**
    @Synopsis it's a example of  extra function for change current encoder to 265
             format.  some extra  propertys  or some else  need to be changed
             after encoder changed , so do this here.

    @Param user_data

    @Returns
*/
/* ----------------------------------------------------------------------------*/
static gboolean
change_to265(gpointer user_data)
{
    mediapipe_t *mp = (mediapipe_t *) user_data;
    int ret = 1;
    GstElement *enc3_caps = gst_bin_get_by_name(GST_BIN((mp)->pipeline),
                            "enc3_caps");
    GstCaps *caps =
        gst_caps_from_string("video/x-h265,stream-format=byte-stream,profile=high");
    gst_element_set_state(enc3_caps, GST_STATE_NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, "enc3_caps", "caps", caps, NULL);
    gst_element_set_state(enc3_caps, GST_STATE_PLAYING);
    gst_caps_unref(caps);
    gst_object_unref(enc3_caps);
    GstElement *sink = gst_bin_get_by_name(GST_BIN((mp)->pipeline), "sink3");
    gst_element_set_state(sink, GST_STATE_NULL);
    MEDIAPIPE_SET_PROPERTY(ret, mp, "sink3", "location", "forchange.265", NULL);
    gst_element_set_state(sink, GST_STATE_PLAYING);
    gst_object_unref(sink);
    return (ret == 0);
}


/* --------------------------------------------------------------------------*/
/**
    @Synopsis  current encoding src pad prob callback for change format.
              this callback is called when listen a eos envent,
              in the callback ,create a new encoding to replace the old one

    @Param pad
    @Param info
    @Param user_data    the context for change format

    @Returns
*/
/* ----------------------------------------------------------------------------*/
static GstPadProbeReturn
change_format_event_probe_cb(GstPad *pad, GstPadProbeInfo *info,
                             gpointer user_data)
{
    if (GST_EVENT_TYPE(GST_PAD_PROBE_INFO_DATA(info)) != GST_EVENT_EOS) {
        return GST_PAD_PROBE_PASS;
    }

    /* remove the probe first */
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    change_format_context_t *ctx = (change_format_context_t *) user_data;
    g_print("formart_string :%s", ctx->format_string);
    /* create encoder */
    GstElement *new_elem = NULL;

    if (!strcmp(ctx->format_string, "H264")) {
        new_elem = gst_element_factory_make("mfxh264enc", NULL);
    } else if (!strcmp(ctx->format_string, "H265")) {
        new_elem = gst_element_factory_make("mfxhevcenc", NULL);
    } else {
        g_print("unsupported format changing\n");
    }

    GstElement *pipeline = ctx->mp->pipeline;
    /* remove unlinks automatically */
    gst_element_set_state(ctx->cur_element, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(pipeline), ctx->cur_element);
    /* add new encoder element */
    gst_element_set_name(new_elem, ctx->enc_name);
    gst_bin_add(GST_BIN(pipeline), new_elem);
    /* some caps and filesink property changed by using user's  extrafun */
    ctx->extra_fun(ctx->user_data);
    /* link element */
    gst_element_link_many(ctx->up_element, new_elem, ctx->down_element, NULL);
    gst_element_set_state(new_elem, GST_STATE_PLAYING);
    /* destroy context */
    g_free(ctx->format_string);
    g_free(ctx->enc_name);
    gst_object_unref(ctx->up_element);
    gst_object_unref(ctx->down_element);
    gst_object_unref(ctx->cur_element);
    g_free(ctx);
    return GST_PAD_PROBE_DROP;
}

/* --------------------------------------------------------------------------*/
/**
    @Synopsis     the up_element src pad prob block callback for change format.
                 in this callback , add a eos linsten on current encoding src pad
                 and  send a eos event to  current encoding sink pad

    @Param pad
    @Param info
    @Param user_data    the context point for change format

    @Returns
*/
/* ----------------------------------------------------------------------------*/
static GstPadProbeReturn
change_format_pad_probe_cb(GstPad *pad, GstPadProbeInfo *info,
                           gpointer user_data)
{
    change_format_context_t *ctx = (change_format_context_t *) user_data;
    /* remove the probe first */
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));
    /* install new probe for EOS */
    GstPad *srcpad = gst_element_get_static_pad(ctx->cur_element, "src");
    gst_pad_add_probe(srcpad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
                      change_format_event_probe_cb, user_data, NULL);
    gst_object_unref(srcpad);
    /*  push EOS into the element, the probe will be fired when the
        EOS leaves the effect and it has thus drained all of its data */
    GstPad *sinkpad = gst_element_get_static_pad(ctx->cur_element, "sink");
    gst_pad_send_event(sinkpad, gst_event_new_eos());
    gst_object_unref(sinkpad);
    return GST_PAD_PROBE_OK;
}


