/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */
#include "mediapipe_com.h"

#include <boost/filesystem.hpp>
#include <fstream>
#include <gst/app/app.h>
#include <set>
#include <string>
#include <vector>

using PacketArray = std::vector<std::pair<std::string, std::string>>;

typedef struct {
    GstElement* appsrc;
    guint sourceid;
    GSource* idle_source;
    GMainContext* loop_context;
    std::string packet_folder;
    PacketArray packets;
    int index;
} feeder_ctx_t;

static char* parse_config(mediapipe_t*, mp_command_t*);
static mp_int_t init_callback(mediapipe_t*);
static void* create_ctx(mediapipe_t*);
static void destroy_ctx(void*);

static mp_command_t mp_feeder_commands[] = {
    mp_custom_command2("feeder", parse_config),
    mp_null_command
};

static mp_module_ctx_t mp_feeder_module_ctx = {
    mp_string("feeder"),
    create_ctx,
    nullptr,
    destroy_ctx
};

mp_module_t mp_feeder_module = {
    MP_MODULE_V1,
    &mp_feeder_module_ctx, /* module context */
    mp_feeder_commands, /* module directives */
    MP_CORE_MODULE, /* module type */
    nullptr, /* init master */
    nullptr, /* init module */
    nullptr, /* keyshot_process*/
    nullptr, /* message_process */
    init_callback, /* init_callback */
    nullptr, /* netcommand_process */
    nullptr, /* exit master */
    MP_MODULE_V1_PADDING
};

static void* create_ctx(mediapipe_t* mp)
{
    feeder_ctx_t* ctx = g_new0(feeder_ctx_t, 1);
    ctx->index = 0;
    ctx->loop_context = g_main_context_get_thread_default();
    return ctx;
}

static void destroy_ctx(void* _ctx)
{
    feeder_ctx_t* ctx = reinterpret_cast<feeder_ctx_t*>(_ctx);

    if (ctx->appsrc) {
        gst_object_unref(ctx->appsrc);
    }

    g_free(ctx);
}

static std::set<std::string> get_file_list(const std::string& path)
{
    std::set<std::string> result;

    for (auto& entry : boost::filesystem::directory_iterator(path)) {
        result.insert(entry.path().string());
    }

    return result;
}

static std::string read_file(const std::string& file)
{
    std::string data;

    std::ifstream in(file, std::ios::in | std::ios::binary);
    if (in) {
        in.seekg(0, std::ios::end);
        data.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&data[0], data.size());
        in.close();
    }

    return data;
}

PacketArray read_files(const std::set<std::string>& files)
{
    PacketArray result;

    for (auto& file : files) {
        auto data = read_file(file);
        if (!data.empty()) {
            result.push_back(std::make_pair(file, std::move(data)));
        } else {
            LOG_WARNING("No content read from file \"%s\"", file.c_str());
        }
    }

    return result;
}

static char* parse_config(mediapipe_t* mp, mp_command_t* cmd)
{
    feeder_ctx_t* ctx = (feeder_ctx_t*)mp_modules_find_moudle_ctx(mp, "feeder");

    const char* packet_folder = nullptr;
    struct json_object* feederconf = nullptr;

    if (!(json_object_object_get_ex(mp->config, "appsrc_feeder", &feederconf)) || !(json_get_string(feederconf, "packet_folder", &packet_folder))) {
        LOG_WARNING("appsrc_feeder: cannot find \"%s\", disable feeder feature", packet_folder);
        return MP_CONF_OK;
    }

    if (packet_folder) {
        ctx->packet_folder = packet_folder;
    }

    return MP_CONF_OK;
}

static gboolean push_data(feeder_ctx_t* ctx)
{
    auto& packet_data = ctx->packets[ctx->index].second;

    GstBuffer* buffer = gst_buffer_new_allocate(NULL, packet_data.size(), NULL);
    if (!buffer) {
        LOG_ERROR("feeder: create buffer failed");
        return FALSE;
    }

    GstMapInfo info;
    gst_buffer_map(buffer, &info, GST_MAP_WRITE);
    memcpy(info.data, packet_data.data(), info.size);
    gst_buffer_unmap(buffer, &info);

    GstFlowReturn ret;
    g_signal_emit_by_name(ctx->appsrc, "push-buffer", buffer, &ret);
    gst_buffer_unref(buffer);
    if (ret != GST_FLOW_OK) {
        LOG_ERROR("feeder: feed buffer failed");
    } else {
        ++ctx->index;
        if (ctx->index >= ctx->packets.size()) {
            ctx->index = 0;
        }
    }

    return TRUE;
}

static void start_feed(GstElement* appsrc, guint unused_size, feeder_ctx_t* ctx)
{
    if (!ctx->sourceid) {
#if 0
        ctx->idle_source = g_idle_source_new();
        g_source_set_callback(ctx->idle_source, (GSourceFunc)push_data, ctx, NULL);
        ctx->sourceid = g_source_attach(ctx->idle_source, ctx->loop_context);
#else
        ctx->sourceid = g_idle_add((GSourceFunc)push_data, ctx);
#endif
    }
}

static void stop_feed(GstElement* appsrc, guint unused_size, feeder_ctx_t* ctx)
{
    if (!ctx->sourceid) {
#if 0
        g_source_destroy(ctx->idle_source);
#else
        g_source_remove(ctx->sourceid);
#endif
        ctx->sourceid = 0;
    }
}

static mp_int_t init_callback(mediapipe_t* mp)
{
    auto ctx = (feeder_ctx_t*)mp_modules_find_moudle_ctx(mp, "feeder");

    if (ctx->packet_folder.empty()) {
        return MP_OK;
    }

    ctx->appsrc = gst_bin_get_by_name(GST_BIN(mp->pipeline), "src0");
    if (!ctx->appsrc) {
        LOG_INFO("feeder: element named \"src\" not found, disable feeding feature");
        return MP_OK;
    }

    if (!GST_IS_APP_SRC(ctx->appsrc)) {
        LOG_INFO("feeder: appsrc named \"src\" not found, disable feeding feature");
        gst_object_unref(ctx->appsrc);
        ctx->appsrc = 0;
        return MP_OK;
    }

    auto files = get_file_list(ctx->packet_folder);
    ctx->packets = read_files(files);

    if (ctx->packets.empty()) {
        LOG_WARNING("appsrc_feeder: no valid packet found under \"%s\", disable feeder feature", ctx->packet_folder.c_str());
        gst_object_unref(ctx->appsrc);
        ctx->appsrc = 0;
        return MP_OK;
    }

    LOG_INFO("feeder: packet_folder=\"%s\", num_files=%lu", ctx->packet_folder.c_str(), ctx->packets.size());

    g_object_set(G_OBJECT(ctx->appsrc), "stream-type", 0, "format", GST_FORMAT_TIME, NULL);
    g_signal_connect(ctx->appsrc, "need-data", G_CALLBACK(start_feed), ctx);
    g_signal_connect(ctx->appsrc, "enough-data", G_CALLBACK(stop_feed), ctx);

    return MP_OK;
}
