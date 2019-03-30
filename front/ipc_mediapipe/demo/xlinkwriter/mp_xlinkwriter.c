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
        , numObject(1)
        , channelId(0x400)
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

static mp_command_t mp_xlinkwriter_commands[] = {
    mp_custom_command0("xlinkwriter"),
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
    nullptr,                       /* init module */
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

#pragma pack(push)
#pragma pack(1)
typedef struct {
    unsigned u : 24;
} Position;
#pragma pack(pop)

typedef struct {
    guint8 reserved;
    guint8 object_id;
    guint16 classfication_GT;
    Position left;
    Position top;
    Position width;
    Position height;
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
    header->meta_size = ctx->headerSize - sizeof(Header);
    header->package_size = ctx->headerSize;

    auto metaData = reinterpret_cast<Metadata*>(pData + sizeof(Header));
    metaData->version = 1;
    metaData->packet_type = 1;
    metaData->stream_id = 1;
    metaData->of_objects = ctx->numObject;
    metaData->frame_number = 0;

    auto border = reinterpret_cast<ObjectBorder*>(pData + sizeof(Header) + sizeof(Metadata));
    for (int i = 0; i < ctx->numObject; ++i) {
        border->top.u = 50;
        border->left.u = 50;
        border->width.u = 50;
        border->height.u = 50;
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

    LOG_DEBUG("[id:%u] %02X %02X %02X %02X", metaData->frame_number, data[64], data[128], data[256], data[512]);
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

        LOG_DEBUG("XLinkReadData success, readBytes=%u", msg_size);

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

    auto status = XLinkInitialize(&ctx->ghandler);
    if (status != X_LINK_SUCCESS) {
        LOG_ERROR("XLinkInitialize failed, status=%d", status);
        delete ctx;
        return nullptr;
    }

    LOG_DEBUG("XLinkInitialize success");

    auto handler = &ctx->handler;
    auto channelId = ctx->channelId;

    status = XLinkBootRemote(handler, DEFAULT_NOMINAL_MAX);
    if (status != X_LINK_SUCCESS) {
        LOG_ERROR("XLinkBootRemote failed, status=%d", status);
        delete ctx;
        return nullptr;
    }

    LOG_DEBUG("XLinkBootRemote success");

    status = XLinkConnect(handler);
    if (status != X_LINK_SUCCESS) {
        LOG_ERROR("XLinkConnect failed, status=%d", status);
        delete ctx;
        return nullptr;
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
        return nullptr;
    }

    LOG_DEBUG("XLinkOpenChannel success");

    init_package(ctx);

    ctx->xlinkreader = std::thread(read_xlink_routine, ctx);

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
