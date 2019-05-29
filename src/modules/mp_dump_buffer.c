#include "mediapipe_com.h"

#include <fstream>
#include <gst/app/app.h>
#include <iomanip>
#include <sstream>
#include <string>

typedef struct {
    int index;
    int maxCount;
    std::string dumpPath;
    std::string dumpPrefix;
    std::string dumpPostfix;
    std::string elemName;
    std::string padName;
} dump_buffer_ctx_t;

static void* create_ctx(mediapipe_t* mp);
static void destroy_ctx(void* _ctx);
static mp_int_t init_callback(mediapipe_t* mp);

static mp_command_t mp_dump_buffer_commands[] = {
    mp_custom_command0("dump_buffer"),
    mp_null_command
};

static mp_module_ctx_t mp_dump_buffer_context = {
    mp_string("dump_buffer"),
    create_ctx,
    nullptr,
    destroy_ctx
};

mp_module_t mp_dump_buffer_module = {
    MP_MODULE_V1,
    &mp_dump_buffer_context,
    mp_dump_buffer_commands,
    MP_CORE_MODULE,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    init_callback,
    nullptr,
    nullptr,
    MP_MODULE_V1_PADDING
};

static void* create_ctx(mediapipe_t* mp)
{
    dump_buffer_ctx_t* ctx = g_new0(dump_buffer_ctx_t, 1);

    ctx->index = 0;
    ctx->maxCount = -1;
    ctx->elemName = "src0";
    ctx->padName = "src";
    ctx->dumpPath = ".";
    ctx->dumpPrefix = "frame_";
    ctx->dumpPostfix = ".binary";

    return ctx;
}

static void destroy_ctx(void* _ctx)
{
    if (_ctx) {
        g_free(_ctx);
    }
}

static gboolean dump_buffer(mediapipe_t* mp, GstBuffer* buffer, guint8* data, gsize size, gpointer user_data)
{
    GstMapInfo info;
    if (!gst_buffer_map(buffer, &info, GST_MAP_READ)) {
        LOG_ERROR("dump_buffer: gst_buffer_map failed!");
        return FALSE;
    }

    dump_buffer_ctx_t* ctx = (dump_buffer_ctx_t*)user_data;

    std::stringstream stream;
    stream << ctx->dumpPath << '/' << ctx->dumpPrefix << std::setw(3) << std::setfill('0') << ctx->index++ << ctx->dumpPostfix;

    std::string filename = stream.str();
    std::ofstream out(filename, std::ios::out | std::ios::binary);
    if (out) {
        auto ptr = reinterpret_cast<const char*>(info.data);
        out.write(ptr, info.size);
        out.close();
        LOG_DEBUG("dump buffer to file \"%s\" success", filename.c_str());
    } else {
        LOG_ERROR("dump buffer: open file \"%s\" failed", filename.c_str());
    }

    if (ctx->maxCount > 0 && ctx->index >= ctx->maxCount) {
        mediapipe_remove_user_callback(mp, ctx->elemName.c_str(), ctx->padName.c_str(), dump_buffer, ctx);
    }

    gst_buffer_unmap(buffer, &info);

    return TRUE;
}

static mp_int_t init_callback(mediapipe_t* mp)
{
    auto ctx = (dump_buffer_ctx_t*) mp_modules_find_module_ctx(mp, "dump_buffer");
    mediapipe_set_user_callback(mp, ctx->elemName.c_str(), ctx->padName.c_str(), dump_buffer, ctx);
    return MP_OK;
}
