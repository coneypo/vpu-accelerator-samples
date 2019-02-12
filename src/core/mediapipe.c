/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

/**
    @file mediapipe.c
    @brief: API for mediapipe
    @author zhao bob,zhaob1@intel.com
    @version 1.0.0
    @date 2017-01-01
    @history:
*/

#include "mediapipe.h"

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis get width and height for caps
 *
 * @Param caps
 * @Param width  the variable store width
 * @Param height the variable store height
*/
/* ----------------------------------------------------------------------------*/
void
get_resolution_from_caps(GstCaps *caps, gint *width, gint *height)
{
    g_assert(caps);
    const GstStructure *str = gst_caps_get_structure(caps, 0);

    if (!gst_structure_get_int(str, "width", width) ||
        !gst_structure_get_int(str, "height", height)) {
        g_print("No width/height available\n");
        return;
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis  call users callback when buffer come
 *
 * @Param pad  the pad of the element
 * @Param info
 * @Param user_data probe context
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static GstPadProbeReturn
probe_callback_user(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    probe_context_t *ctx = (probe_context_t *)user_data;

    if (ctx->user_callback) {
        GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
        buffer = gst_buffer_make_writable(buffer);
        GST_PAD_PROBE_INFO_DATA(info) = buffer;
        if (ctx->user_callback) {
            ctx->user_callback(ctx->mp, buffer, NULL, 0, ctx->user_data);
        } else {
            ctx->mp->probe_data_list = g_list_remove(ctx->mp->probe_data_list, user_data);
            g_free(user_data);
            return GST_PAD_PROBE_REMOVE;
        }

    }

    return GST_PAD_PROBE_OK;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis create context for callback
 *
 * @Param mp  the main pipeline ctx
 * @Param elem_name  the element when this callback happen on
 * @Param pad_name  the pad of the element when this callback happen on
 *
 * @Returns  prob context pointer
 */
/* ----------------------------------------------------------------------------*/
probe_context_t *
create_callback_context(mediapipe_t *mp, const gchar *elem_name,
                        const gchar *pad_name)
{
    GstElement *elem = gst_bin_get_by_name(GST_BIN(mp->pipeline), elem_name);

    if (!elem) {
        LOG_WARNING("Add callback failed, can not find element \"%s\"", elem_name);
        return NULL;
    }

    probe_context_t *ctx = g_new0(probe_context_t, 1);

    if (ctx) {
        ctx->mp = mp;
        ctx->element = elem;
        ctx->pad_name = pad_name;
        mp->probe_data_list = g_list_append(mp->probe_data_list, ctx);
    }

    gst_object_unref(elem);
    return ctx;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis  add callback for buffer envent
 *
 * @Param probe_callback the callback
 * @Param ctx    the content of this callback
 *
 * @Returns  true return 0
 */
/* ----------------------------------------------------------------------------*/
int
add_probe_callback(GstPadProbeCallback probe_callback, probe_context_t *ctx)
{
    GstPad *pad = gst_element_get_static_pad(ctx->element, ctx->pad_name);

    if (pad) {
        gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, probe_callback, ctx, NULL);
        gst_object_unref(pad);
    }

    return 0;
}

/**
    @brief Set the user callback function

    @param mp Pointer to medipipe.
    @param element_name Add callback to sink pad or src pad of this element.
    @param pad_name "sink" for sink pad, "src" for src pad
    @param user_callback Pointer to user callback function.
    @param user_data Data pass to user callback function.

    @retval 0: Success
    @retval -1: Fail
*/
int
mediapipe_set_user_callback(mediapipe_t *mp, const gchar *elem_name,
                            const gchar *pad_name, user_callback_t user_callback, gpointer user_data)
{
    if (!mp || !elem_name || !pad_name || mp->state == STATE_NOT_CREATE) {
        return -1;
    }

    probe_context_t *ctx = create_callback_context(mp, elem_name, pad_name);

    if (!ctx) {
        return -1;
    }

    ctx->user_callback = user_callback;
    ctx->user_data = user_data;
    return add_probe_callback(probe_callback_user, ctx);
}

/**
    @brief remove the user callback function

    @param mp Pointer to medipipe.
    @param element_name remove callback to sink pad or src pad of this element.
    @param pad_name "sink" for sink pad, "src" for src pad
    @param user_callback Pointer to user callback function.
    @param user_data Data pass to user callback function.

    @retval 0: Success
    @retval -1: Fail
*/
int
mediapipe_remove_user_callback(mediapipe_t *mp,
                               const gchar *elem_name, const gchar *pad_name,
                               user_callback_t user_callback, gpointer user_data)
{
    if (!mp || !elem_name || !pad_name || mp->state == STATE_NOT_CREATE) {
        return -1;
    }
    GstElement *elem = gst_bin_get_by_name(GST_BIN(mp->pipeline), elem_name);
    if (!elem) {
        LOG_WARNING("remove callback failed, can not find element \"%s\"", elem_name);
        return -1;
    }
    GList *l = mp->probe_data_list;
    while (l != NULL) {
        probe_context_t *ctx = l->data;
        if (ctx->element == elem
            && 0 == g_strcmp0(ctx->pad_name, pad_name)
            && ctx->user_callback == user_callback
            && ctx->user_data == user_data) {
            ctx->user_callback = NULL;
            break;
        }
        l = l->next;
    }
    gst_object_unref(elem);
    if (l == NULL) {
        return -1;
    }
    return 0;
}





/* --------------------------------------------------------------------------*/
/**
 * @Synopsis main pipeline gst_messge progress  callback
 *
 * @Param bus the bus
 * @Param msg the message
 * @Param data   user data
 *
 * @Returns      return TRUE
 */
/* ----------------------------------------------------------------------------*/
static gboolean
bus_callback(GstBus *bus, GstMessage *msg, gpointer data)
{
    mediapipe_t *mp = (mediapipe_t *) data;

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError *err = NULL; /* error to show to users                 */
        gchar *dbg = NULL;  /* additional debug string for developers */
        gst_message_parse_error(msg, &err, &dbg);

        if (err) {
            LOG_ERROR("ERROR: %s\n", err->message);
            g_error_free(err);
        }

        if (dbg) {
            LOG_ERROR("[Debug details: %s]\n", dbg);
            g_free(dbg);
        }

        mediapipe_stop(mp);
        break;
    }

    case GST_MESSAGE_EOS:
        /* end-of-stream */
        LOG_DEBUG("EOS\n");
        mediapipe_stop(mp);
        break;

    default:
        /* unhandled message */
        //LOG_DEBUG ("unhandled message");
        mp_modules_message_process(mp, msg);
        break;
    }

    /*  we want to be notified again the next time there is a message
        on the bus, so returning TRUE (FALSE means we want to stop watching
        for messages on the bus and our callback should not be called again)
    */
    return TRUE;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis create pipeline from string
 *
 * @Param data pipeline string
 *
 * @Returns  return the pipeline element
 */
/* ----------------------------------------------------------------------------*/
static inline GstElement *
create_pipeline_from_string(const gchar *data)
{
    g_assert(data != NULL);
    GError     *error = NULL;
    GstElement *pipeline = NULL;
    pipeline = gst_parse_launch(data, &error);
    if (error || !pipeline) {
        LOG_ERROR("failed to build pipeline from string : %s, error message: %s",
                  data, (error) ? error->message : NULL);
        return NULL;
    }
    return pipeline;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis create pipeline from file
 *
 * @Param file the file contain pipeline string
 *
 * @Returns  return the pipeline element
 */
/* ----------------------------------------------------------------------------*/
static inline GstElement *
create_pipeline_from_file(const char *file)
{
    gchar      *data = NULL;
    GError     *error = NULL;
    GstElement *pipeline = NULL;

    if (!(data = read_file(file))) {
        return NULL;
    }

    pipeline = gst_parse_launch(data, &error);
    g_free(data);

    if (error || !pipeline) {
        g_print("failed to build pipeline from: %s error message: %s\n", file,
                (error) ? error->message : NULL);
        return NULL;
    }

    return pipeline;
}

/**
    @brief Create mediapipe.

    @param argc Argument from main() function.
    @param argv[] Argument from main() function.
    @param launch_file_name The file name of gstreamer launch script.

    @returns The pointer to the created mediapipe or NULL if failed.
*/
mediapipe_t *
mediapipe_create(int argc, char *argv[])
{
    if (!parse_cmdline(argc, argv)) {
        return NULL;
    }

    mediapipe_t *mp = g_new0(mediapipe_t, 1);
    gst_init(&argc, &argv);
    mp->pipeline = create_pipeline_from_file(g_launch_filename);

    if (!mp->pipeline) {
        g_free(mp);
        return NULL;
    }

    /* json_setup_elements (mp, mp->config); */
    /* json_setup_rtsp_server (mp, mp->config); */
    GstBus *bus = gst_element_get_bus(mp->pipeline);
    mp->bus_watch_id = gst_bus_add_watch(bus, bus_callback, mp);
    gst_object_unref(bus);
    mp->loop = g_main_loop_new(NULL, FALSE);
    mp->state = STATE_READY;
    /* json_setup_cvsdk_branch (mp, mp->config); */
    mp->config = json_create(g_config_filename);

    if (!mp->config) {
        g_print("failed to load config information from: %s\n", g_config_filename);
        g_free(mp);
        return NULL;
    }

    return mp;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis  init mediapipe from config string and launch string
 *
 * @Param config     config string
 * @Param launch     launch string
 * @Param mp        mediapipe
 *
 * @Returns  if success return true
 */
/* ----------------------------------------------------------------------------*/
gboolean
mediapipe_init_from_string(const char *config, const char *launch, mediapipe_t *mp)
{
    gst_init(0, NULL);
    g_assert(mp != NULL);
    g_assert(config != NULL);
    g_assert(launch != NULL);
    if (mp->pipeline != NULL) {
        LOG_ERROR("mediapipe already have a pipeline");
        return FALSE;
    }

    mp->pipeline = create_pipeline_from_string(launch);
    if (!mp->pipeline) {
        LOG_ERROR("failed to create pipeline from string");
        return FALSE;
    }

    mp->config = json_create_from_string(config);
    if (!mp->config) {
        LOG_ERROR("failed to load config from string");
        return FALSE;
    }

    GstBus *bus = gst_element_get_bus(mp->pipeline);
    mp->bus_watch_id = gst_bus_add_watch(bus, bus_callback, mp);
    gst_object_unref(bus);
    mp->loop = g_main_loop_new(NULL, FALSE);
    mp->state = STATE_READY;
    return TRUE;
}

/**
    @brief Destroy the structure of mediapipe.

    @param mp Pointer to mediapipe.
*/
void
mediapipe_destroy(mediapipe_t *mp)
{
    g_assert(mp);
    g_source_remove(mp->bus_watch_id);
    gst_object_unref(mp->pipeline);
    g_main_loop_unref(mp->loop);
    json_destroy(&mp->config);
    mp_modules_exit_master(mp);
    g_list_free_full(mp->probe_data_list, (GDestroyNotify) g_free);
    g_free(mp);
}

/**
    @brief Start mediapipe. Thread will loop in this function until calling mediapipe_stop().

    @param mp Pointer to mediapipe.
*/
void
mediapipe_start(mediapipe_t *mp)
{
    g_assert(mp);
    gst_element_set_state(mp->pipeline, GST_STATE_PLAYING);
    mp->state = STATE_START;
    g_main_loop_run(mp->loop);
}

/**
    @brief Stop mediapipe.

    @param mp Pointer to mediapipe.
*/
void
mediapipe_stop(mediapipe_t *mp)
{
    g_assert(mp);
    gst_element_set_state(mp->pipeline, GST_STATE_NULL);
    mp->state = STATE_READY;
    g_main_loop_quit(mp->loop);
}

/**
    @brief Resume mediapipe to playing.

    @param mp Pointer to mediapipe.
*/
void
mediapipe_playing(mediapipe_t *mp)
{
    g_assert(mp);
    gst_element_set_state(mp->pipeline, GST_STATE_PLAYING);
    mp->state = STATE_START;
}

/**
    @brief Pause the mediapipe.

    @param mp Pointer to mediapipe.
*/
void
mediapipe_pause(mediapipe_t *mp)
{
    g_assert(mp);
    gst_element_set_state(mp->pipeline, GST_STATE_NULL);
    mp->state = STATE_READY;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis   setup a new branch pipeline
 *
 * @Param mp     the main pipeline ctx
 * @Param element_name  the element provide input buffer for new branch
 * @Param pad_name the pad of the element that provide buffer
 * @Param branch   new branch
 */
/* ----------------------------------------------------------------------------*/
gboolean
mediapipe_setup_new_branch(mediapipe_t *mp, const gchar *element_name,
                           const gchar *pad_name, mediapipe_branch_t *branch)
{
    gboolean success = FALSE;

    if (!mp || !element_name || !pad_name || !branch) {
        return FALSE;
    }

    GstElement *element = gst_bin_get_by_name(GST_BIN(mp->pipeline),
                          element_name);

    if (!element) {
        return FALSE;
    }

    GstPad *pad = gst_element_get_static_pad(element, pad_name);
    GstCaps *caps = gst_pad_get_allowed_caps(pad);
    gint width = 0, height = 0;
    get_resolution_from_caps(caps, &width, &height);

    if (width > 0 && height > 0) {
        branch->input_width = width;
        branch->input_height = height;
        /* success = mediapipe_branch_init (branch); */
        success = branch->branch_init(branch);

        if (!success) {
            goto faild;
        }

        success = mediapipe_branch_start(branch);

        if (!success) {
            mediapipe_branch_destroy_internal(branch);
            goto faild;
        }
    } else {
        g_print("width or height is not biggen than 0 when create \
                branch from %s and pad %s\n", element_name , pad_name);
    }

faild:
    gst_object_unref(element);
    gst_object_unref(pad);
    gst_caps_unref(caps);
    return success;
}


