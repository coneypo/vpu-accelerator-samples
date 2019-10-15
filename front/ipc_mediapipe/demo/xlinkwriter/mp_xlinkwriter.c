/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <gst/app/app.h>
#include <thread>
#include <vector>

#include "XLink.h"
#include "mediapipe_com.h"

struct XLinkWriterContext {
public:
    XLinkWriterContext()
        : frameIndex(0)
        , headerSize(0)
        , numObject(3)
        , channelId(0x401)
        , opMode(RXB_TXB)
    {
        handler.devicePath = (char*)"/tmp/xlink_mock";
        handler.deviceType = PCIE_DEVICE;
        ghandler.protocol = PCIE;
    }

public:
    guint32 frameIndex;
    uint32_t headerSize;
    std::vector<uint8_t> package;
    guint8 numObject;
    channelId_t channelId;
    OperationMode_t opMode;
    XLinkHandler_t handler;
    XLinkGlobalHandler_t ghandler;
    std::thread xlinkreader;
    static constexpr uint32_t fragmentSize = 1024;
};

static mp_int_t init_callback(mediapipe_t* mp);
static void* create_context(mediapipe_t* mp);
static void destroy_context(void* context);
static mp_int_t init_module(mediapipe_t *mp);
static char *
mp_parse_config(mediapipe_t *mp, mp_command_t *cmd);

static mp_command_t mp_xlinkwriter_commands[] = {
    {
        mp_string("xlinkwriter"),
        MP_MAIN_CONF,
        mp_parse_config,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_module_ctx_t mp_xlinkwriter_module_ctx = {
    mp_string("xlinkwriter"),
    create_context,
    nullptr,
    destroy_context
};

mp_module_t mp_xlinkwriter_module = {
    MP_MODULE_V1,
    &mp_xlinkwriter_module_ctx,    /* module context */
    mp_xlinkwriter_commands,       /* module directives */
    MP_CORE_MODULE,                /* module type */
    nullptr,                       /* init master */
    init_module,                       /* init module */
    nullptr,                       /* keyshot_process*/
    nullptr,                       /* message_process */
    init_callback,                 /* init_callback */
    nullptr,                       /* netcommand_process */
    nullptr,                       /* exit master */
    MP_MODULE_V1_PADDING
};

typedef struct {
    guint8 magic;
    guint8 version;
    guint16 meta_size;
    guint32 package_size;
} Header;

typedef struct {
    guint8 version;
    guint8 packet_type;
    guint8 stream_id;
    guint8 of_objects;
    guint32 frame_number;
} Metadata;

typedef struct {
    guint8 reserved;
    guint8 object_id;
    guint16 classfication_GT;
    guint16 left;
    guint16 top;
    guint16 width;
    guint16 height;
    guint32 reserved2;
} ObjectBorder;

static void init_package(XLinkWriterContext* ctx)
{
    if (!ctx) {
        return;
    }

    ctx->headerSize = sizeof(Header) + sizeof(Metadata) + ctx->numObject * sizeof(ObjectBorder);
    ctx->package.resize(ctx->headerSize, 0);

    auto pData = ctx->package.data();

    auto header = reinterpret_cast<Header*>(pData);
    header->magic = 0xA;
    header->version = 1;
    header->meta_size = 8;
    header->package_size = ctx->headerSize;

    auto metaData = reinterpret_cast<Metadata*>(pData + sizeof(Header));
    metaData->version = 1;
    metaData->packet_type = 1;
    metaData->stream_id = 1;
    metaData->of_objects = ctx->numObject;
    metaData->frame_number = 0;

    auto border = reinterpret_cast<ObjectBorder*>(pData + sizeof(Header) + sizeof(Metadata));
    for (int i = 0; i < ctx->numObject; ++i) {
        border->top = 50;
        border->left = 50;
        border->width = 50;
        border->height = 50;
        border += 1;
    }
}

static void copy_to_package(XLinkWriterContext* ctx, const uint8_t* data, const gsize bufferSize)
{
    auto totalSize = ctx->headerSize + bufferSize;
    ctx->package.resize(totalSize);

    auto pData = ctx->package.data();

    auto header = reinterpret_cast<Header*>(pData);
    header->package_size = totalSize;

    auto metaData = reinterpret_cast<Metadata*>(pData + sizeof(Header));
    metaData->frame_number = ctx->frameIndex++;

    memcpy(pData + ctx->headerSize, data, bufferSize);
}

static void read_xlink_routine(XLinkWriterContext* ctx)
{
    auto handler = &ctx->handler;
    auto channelId = ctx->channelId;

    while (true) {
        uint32_t msg_size = 0;
        uint8_t* message = nullptr;

        auto status = XLinkReadData(handler, channelId, &message, &msg_size);
        if (status != X_LINK_SUCCESS) {
            LOG_ERROR("XLinkReadData failed, status=%d", status);
            return;
        }

        LOG_DEBUG("XLinkReadData success,channel:%d, readBytes=%u",channelId, msg_size);

        status = XLinkReleaseData(handler, channelId, message);
        if (status != X_LINK_SUCCESS) {
            LOG_ERROR("XLinkReleaseData failed, status=%d", status);
            return;
        }
    }
}

void* create_context(mediapipe_t* mp)
{
    auto ctx = new XLinkWriterContext();
    return ctx;
}

void destroy_context(void* context)
{
    auto ctx = reinterpret_cast<XLinkWriterContext*>(context);
    if (!ctx) {
        LOG_WARNING("cast to XLinkWriterContext pointer failed, input=%p", context);
        return;
    }

    LOG_DEBUG("do XLinkCloseChannel");
    XLinkCloseChannel(&ctx->handler, ctx->channelId);
    XLinkDisconnect(&ctx->handler);

    if (ctx->xlinkreader.joinable()) {
        ctx->xlinkreader.join();
    }

    delete ctx;
}

static gboolean process_data(XLinkWriterContext* ctx, const uint8_t* data, const gsize size)
{
    copy_to_package(ctx, data, size);

    auto handler = &ctx->handler;
    auto channelId = ctx->channelId;
    auto& package = ctx->package;

    auto status = XLinkWriteData(handler, channelId, package.data(), package.size());
    if (status != X_LINK_SUCCESS) {
        LOG_ERROR("xlinkwriter: XLinkWriteData failed, status=%d", status);
        return FALSE;
    }

    LOG_DEBUG("XLinkWriteData success, frameIndex-%u frameSize=%lu", ctx->frameIndex-1, package.size());

    return TRUE;
}

static gboolean
proc_src_callback(mediapipe_t* mp, GstBuffer* buf, guint8* data, gsize size, gpointer user_data)
{
    GstMapInfo info;
    if (!gst_buffer_map(buf, &info, GST_MAP_READ)) {
        LOG_ERROR("xlinkwriter: gst_buffer_map failed");
        return FALSE;
    }

    auto ctx = (XLinkWriterContext*)user_data;
    auto ret = process_data(ctx, info.data, info.size);

    gst_buffer_unmap(buf, &info);

    return ret;
}

static mp_int_t init_callback(mediapipe_t* mp)
{
    auto ctx = mp_modules_find_moudle_ctx(mp, "xlinkwriter");
    if (!ctx) {
        LOG_ERROR("xlinkwriter: find module context failed");
        return MP_ERROR;
    }

    mediapipe_set_user_callback(mp, "proc_src", "src", proc_src_callback, ctx);

    return MP_OK;
}

static char *
mp_parse_config(mediapipe_t *mp, mp_command_t *cmd)
{

    guint channel = 0x400;
    GstElement  *xlinksrc = NULL;
    struct json_object *xlinkconf = NULL;

    auto ctx = reinterpret_cast<XLinkWriterContext*>(mp_modules_find_moudle_ctx(mp, "xlinkwriter"));
    if (!ctx) {
        LOG_ERROR("xlinkwriter: find module context failed");
    }

    //TODO:set xlinksrc channel property
    if (!(json_object_object_get_ex(mp->config, "xlink",  &xlinkconf)) ||
        !(json_get_uint(xlinkconf, "channel", &channel))) {
        LOG_WARNING("xlinksrc: can't find channel property use default 1024 !");
    }

    ctx->channelId = channel;

    return MP_CONF_OK;
}

static mp_int_t init_module(mediapipe_t *mp)
{

    XLinkError_t status;
    auto ctx = reinterpret_cast<XLinkWriterContext*>(mp_modules_find_moudle_ctx(mp, "xlinkwriter"));
    if (!ctx) {
        LOG_ERROR("xlinkwriter: find module context failed");
    }
    auto handler = &ctx->handler;
    auto channelId = ctx->channelId;
    static volatile gsize init = 0;
    if (g_once_init_enter(&init)) {
        gsize setup_init;
        status = XLinkInitialize(&ctx->ghandler);
        if (status != X_LINK_SUCCESS) {
            LOG_ERROR("XLinkInitialize failed, status=%d", status);
            delete ctx;
            return MP_ERROR;
        }

        LOG_DEBUG("XLinkInitialize success");


        status = XLinkBootRemote(handler, DEFAULT_NOMINAL_MAX);
        if (status != X_LINK_SUCCESS) {
            LOG_ERROR("XLinkBootRemote failed, status=%d", status);
            delete ctx;
            return MP_ERROR;
        }

        LOG_DEBUG("XLinkBootRemote success");

        status = XLinkConnect(handler);
        if (status != X_LINK_SUCCESS) {
            LOG_ERROR("XLinkConnect failed, status=%d", status);
            delete ctx;
            return MP_ERROR;
        }

        setup_init = 1;
        g_once_init_leave(&init, setup_init);
    }
    LOG_DEBUG("XLinkConnect success");

    for (int i = 0; i < 16; ++i) {
        status = XLinkOpenChannel(handler, channelId, ctx->opMode, XLinkWriterContext::fragmentSize, 0);
        if (status == X_LINK_SUCCESS) {
            break;
        } else {
            LOG_ERROR("%dth XLinkOpenChannel failed, status=%d", i, status);
        }
    }

    if (status != X_LINK_SUCCESS) {
        LOG_ERROR("XLinkOpenChannel failed, status=%d", status);
        delete ctx;
        return MP_ERROR;
    }

    LOG_DEBUG("XLinkOpenChannel success");

    init_package(ctx);

    ctx->xlinkreader = std::thread(read_xlink_routine, ctx);
}
