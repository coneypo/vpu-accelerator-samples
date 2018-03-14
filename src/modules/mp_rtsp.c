#include "../core/mediapipe_com.h"

static char *
mp_rtsp_block(mediapipe_t *mp, mp_command_t *cmd);

static void
mediapipe_rtsp_server_start(mediapipe_t *mp);

static void
json_setup_rtsp_server(mediapipe_t *mp, struct json_object *root);

static GstPadProbeReturn
rtsp_probe_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);

static gboolean
mediapipe_rtsp_server_new(mediapipe_t *mp, const char *element_name,
                          const char *caps_string, gint fps, const char *mount_path);

static void
media_configure(GstRTSPMediaFactory *factory, GstRTSPMedia *media,
                gpointer user_data);

static void
mediapipe_remove_rtsp_mount_point (mediapipe_t *mp, const char* mount_path);

static void
json_new_rtsp_server(mediapipe_t *mp, struct json_object *rtsp_server);

static  mp_int_t
message_process(mediapipe_t *mp, void *message);

static mp_command_t  mp_rtsp_commands[] = {
    {
        mp_string("rtsp"),
        MP_MAIN_CONF,
        mp_rtsp_block,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_core_module_t  mp_rtsp_module_ctx = {
    mp_string("rtsp"),
    NULL,
    NULL
};

mp_module_t  mp_rtsp_module = {
    MP_MODULE_V1,
    &mp_rtsp_module_ctx,                /* module context */
    mp_rtsp_commands,                   /* module directives */
    MP_CORE_MODULE,                       /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    NULL,                    /* keyshot_process*/
    message_process,            /* message_process */
    NULL,                      /* init_callback */
    NULL,                               /* netcommand_process */
    NULL,                               /* exit master */
    MP_MODULE_V1_PADDING
};

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis set up a new rtsp server mount point
 *
 * @Param mp
 * @Param rtsp_server
 */
/* ----------------------------------------------------------------------------*/
static void
json_new_rtsp_server(mediapipe_t *mp, struct json_object *rtsp_server)
{
    int fps = 30;
    const char *element, *caps, *mount_path = "/test0";
    RETURN_IF_FAIL(json_check_enable_state(rtsp_server, "enable"));
    RETURN_IF_FAIL(json_get_string(rtsp_server, "element", &element));
    RETURN_IF_FAIL(json_get_string(rtsp_server, "caps", &caps));
    json_get_int(rtsp_server, "fps", &fps);
    json_get_string(rtsp_server, "mount_path", &mount_path);
    mediapipe_rtsp_server_new(mp, element, caps, fps, mount_path);
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis when there is a client connect ,this fuction set up stream
 *
 * @Param factory
 * @Param media
 * @Param user_data
 */
/* ----------------------------------------------------------------------------*/
static void
media_configure(GstRTSPMediaFactory *factory, GstRTSPMedia *media,
                gpointer user_data)
{
    probe_context_t *ctx = (probe_context_t *)user_data;
    GstElement *element, *appsrc;
    GstCaps *caps;
    element = gst_rtsp_media_get_element(media);
    appsrc = gst_bin_get_by_name(GST_BIN(element), "mysrc");
    gst_object_unref(element);
    /* this instructs appsrc that we will be dealing with timed buffer */
    //gst_util_set_object_arg (G_OBJECT (appsrc), "format", "time");
    g_object_set(G_OBJECT(appsrc), "stream-type", 0, "format", GST_FORMAT_TIME,
                 NULL);
    caps = gst_caps_from_string(ctx->caps_string);
    g_object_set(G_OBJECT(appsrc), "caps", caps, NULL);
    gst_caps_unref(caps);
    ctx->timestamp = 0;
    ctx->src = appsrc;
    /* make sure the data is freed when the media is gone */
    //g_object_set_data_full (G_OBJECT (media), "my-extra-data", ctx,
    //    (GDestroyNotify) g_free);
    add_probe_callback(rtsp_probe_callback, ctx);
}

/**
    @brief Creat a rtsp server.

    @param mp Pointer to mediapipe.
    @param element_name Name of encoder element in this video channel.
    RTSP server will retrieve data from its source pad, then pack it with RTP protocol, send out through network.
    @param caps_string The caps negotiation between encoder element and RTSP server.
    @param fps The frame rate of video. This will impact the timestamp of frame that sended out.
    @param mount_path The mount path of rtsp server such as "/test0". The rtsp url will be "rtsp://ip:port/test0".

    @retval TRUE: Success
    @retval FALSE: Fail
*/
static gboolean
mediapipe_rtsp_server_new(mediapipe_t *mp, const char *element_name,
                          const char *caps_string, gint fps, const char *mount_path)
{
    GstRTSPServer *server;
    GstRTSPMountPoints *mounts;
    GstRTSPMediaFactory *factory;
    probe_context_t *ctx;
    g_assert(mp);
    g_assert(element_name);
    g_assert(caps_string);
    g_assert(mount_path);

    if (!mp->rtsp_server) {
        mp->rtsp_server = gst_rtsp_server_new();
    }

    server = mp->rtsp_server;
    mounts = gst_rtsp_server_get_mount_points(server);
    factory = gst_rtsp_media_factory_new();

    if (strstr(caps_string, "h264")) {
        gst_rtsp_media_factory_set_launch(factory,
                                          "( appsrc name=mysrc ! rtph264pay name=pay0 pt=96 )");
    } else if (strstr(caps_string, "h265")) {
        gst_rtsp_media_factory_set_launch(factory,
                                          "( appsrc name=mysrc ! rtph265pay name=pay0 pt=96 )");
    } else if (strstr(caps_string, "jpeg")) {
        gst_rtsp_media_factory_set_launch(factory,
                                          "( appsrc name=mysrc ! rtpjpegpay name=pay0 pt=96 )");
    } else if (strstr(caps_string, "alaw")) {
        gst_rtsp_media_factory_set_launch(factory,
                                          "( appsrc name=mysrc ! rtppcmapay name=pay0 pt=96 )");
    } else if (strstr(caps_string, "mulaw")) {
        gst_rtsp_media_factory_set_launch(factory,
                                          "( appsrc name=mysrc ! rtppcmupay name=pay0 pt=96 )");
    } else if (strstr(caps_string, "adpcm")) {
        gst_rtsp_media_factory_set_launch(factory,
                                          "( appsrc name=mysrc ! rtpg726pay name=pay0 pt=96 )");
    } else if (strstr(caps_string, "G722")) {
        gst_rtsp_media_factory_set_launch(factory,
                                          "( appsrc name=mysrc ! rtpg722pay name=pay0 pt=96 )");
    } else if (strstr(caps_string, "audio/mpeg")) {
        gst_rtsp_media_factory_set_launch(factory,
                                          "( appsrc name=mysrc ! rtpmp4apay name=pay0 pt=96 )");
    } else {
        LOG_ERROR("Caps for rtsp server is wrong.");
        return FALSE;
    }

    gst_rtsp_media_factory_set_shared(factory, TRUE);
    ctx = create_callback_context(mp, element_name, "src");

    if (!ctx) {
        return FALSE;
    }

    ctx->caps_string = caps_string;
    ctx->fps = fps;
    g_signal_connect(factory, "media-configure", (GCallback) media_configure, ctx);
    gst_rtsp_mount_points_add_factory(mounts, mount_path, factory);
    gst_object_unref(mounts);
    gchar *sport = gst_rtsp_server_get_service(mp->rtsp_server);

    if (sport != NULL) {
        g_print("rtsp address: rtsp://%s:%s%s\n", get_local_ip_addr(), sport,
                mount_path);
        g_free(sport);
    } else {
        g_print("rtsp address: rtsp://%s:8554%s\n", get_local_ip_addr(), mount_path);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis rtsp callback for push buffer to rtsp appsrc
 *
 * @Param pad
 * @Param info
 * @Param user_data
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static GstPadProbeReturn
rtsp_probe_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    probe_context_t *ctx = (probe_context_t *)user_data;
    GstBuffer *buffer = gst_buffer_copy_deep(GST_PAD_PROBE_INFO_BUFFER(info));
    if(ctx->fps > 0) {
        GST_BUFFER_PTS(buffer) = ctx->timestamp;
        GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND,
                ctx->fps);
        ctx->timestamp += GST_BUFFER_DURATION(buffer);
    }
    GstFlowReturn status;
    g_signal_emit_by_name(ctx->src, "push-buffer", buffer, &status);
    gst_buffer_unref(buffer);

    if (status != GST_FLOW_OK) {
        LOG_ERROR("Failed to push buffer to rtsp server!, ret=%d\n", (int)status);
        gst_pad_remove_probe(pad,
                             info->id);     //when media is gone, release source here
        gst_object_unref(ctx->src);
    }

    return GST_PAD_PROBE_OK;
}

/**
    @brief Start all the created rtsp server.

    @param mp Pointer to mediapipe.
*/
static void
mediapipe_rtsp_server_start(mediapipe_t *mp)
{
    if (mp->rtsp_server) {
        gst_rtsp_server_attach(mp->rtsp_server, NULL);
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis parse rtps info from root json object and setup it
 *
 * @Param mp
 * @Param root
 */
/* ----------------------------------------------------------------------------*/
static void
json_setup_rtsp_server(mediapipe_t *mp, struct json_object *root)
{
    struct json_object *rtsp_server, *rtsp;
    struct json_object *rtsp_server_array;
    RETURN_IF_FAIL(mp != NULL);
    RETURN_IF_FAIL(root != NULL);
    RETURN_IF_FAIL(json_object_object_get_ex(root, "rtsp",
                   &rtsp));
    RETURN_IF_FAIL(json_object_object_get_ex(rtsp, "rtsp_server",
                   &rtsp_server_array));
    int port = 0;
    gboolean has_port = json_get_int(rtsp, "rtsp_server_port", &port);

    if (has_port && port > 0 && port <= 65535) {
        char sport[6];
        sprintf(sport, "%d", port);

        if (!mp->rtsp_server) {
            mp->rtsp_server = gst_rtsp_server_new();
        }

        gst_rtsp_server_set_service(mp->rtsp_server, sport);
    }

    int num_server = json_object_array_length(rtsp_server_array);

    for (int i = 0; i < num_server; ++i) {
        rtsp_server = json_object_array_get_idx(rtsp_server_array, i);
        json_new_rtsp_server(mp, rtsp_server);
    }

    mediapipe_rtsp_server_start(mp);
}

static char *
mp_rtsp_block(mediapipe_t *mp, mp_command_t *cmd)
{
    json_setup_rtsp_server(mp, mp->config);
    return MP_CONF_OK;
}

/**
 * @brief Start all the created rtsp server.
 *
 * @param mp Pointer to mediapipe.
 */
static void
mediapipe_remove_rtsp_mount_point(mediapipe_t *mp, const char *mount_path)
{
    GstRTSPServer *server;
    GstRTSPMountPoints *mounts;
    if (NULL != mp->rtsp_server) {
        server = mp->rtsp_server;
        mounts = gst_rtsp_server_get_mount_points(server);
        gst_rtsp_mount_points_remove_factory(mounts, mount_path);
    }
}

static  mp_int_t
message_process(mediapipe_t *mp, void *message)
{
    GstMessage *m = (GstMessage *) message;
    const  GstStructure *s;
    const gchar *ele_name_s;
    const gchar *r_caps_s;
    int  fps = -1;
    const  gchar *mount_path;
    if (GST_MESSAGE_TYPE(m) != GST_MESSAGE_APPLICATION) {
        return MP_IGNORE;
    }
    s = gst_message_get_structure(m);
    const gchar *name = gst_structure_get_name(s);
    if (0 == strcmp(name, "rtsp_restart")) {
        if (gst_structure_get(s,
                              "ele_name_s", G_TYPE_STRING, &ele_name_s,
                              "r_caps_s", G_TYPE_STRING, &r_caps_s,
                              "fps", G_TYPE_INT, &fps,
                              "mount_path", G_TYPE_STRING, &mount_path,
                              NULL)) {
            mediapipe_remove_rtsp_mount_point(mp, mount_path);
            mediapipe_rtsp_server_new(mp, ele_name_s, r_caps_s, fps, mount_path);
        }
        return MP_OK;
    }
    return MP_IGNORE;
}
