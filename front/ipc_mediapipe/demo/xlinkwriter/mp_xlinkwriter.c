#include <gst/app/app.h>
#include <thread>

#include "XLink.h"
#include "mediapipe_com.h"

struct XLinkWriterContext {
public:
    XLinkWriterContext()
        : header(nullptr)
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
    uint8_t* header;
    uint32_t headerSize;
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
    NULL,
    destroy_context
};

mp_module_t mp_xlinkwriter_module = {
    MP_MODULE_V1,
    &mp_xlinkwriter_module_ctx,    /* module context */
    mp_xlinkwriter_commands,       /* module directives */
    MP_CORE_MODULE,                /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* keyshot_process*/
    NULL,                          /* message_process */
    init_callback,                 /* init_callback */
    NULL,                          /* netcommand_process */
    NULL,                          /* exit master */
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

static void init_package_head(XLinkWriterContext* ctx)
{
    if (!ctx) {
        return;
    }

    ctx->headerSize = sizeof(Header) + sizeof(Metadata) + ctx->numObject * sizeof(ObjectBorder);
    ctx->header = new uint8_t[ctx->headerSize];

    auto header = reinterpret_cast<Header*>(ctx->header);
    header->magic = 0xA;
    header->version = 1;
    header->meta_size = ctx->headerSize - sizeof(Header);
    header->package_size = ctx->headerSize;

    auto metaData = reinterpret_cast<Metadata*>(ctx->header + sizeof(Header));
    metaData->version = 1;
    metaData->packet_type = 1;
    metaData->stream_id = 1;
    metaData->of_objects = ctx->numObject;
    metaData->frame_number = 0;

    auto border = reinterpret_cast<ObjectBorder*>(ctx->header + sizeof(Header) + sizeof(Metadata));
    for (int i = 0; i < ctx->numObject; ++i) {
        border->top.u = 50;
        border->left.u = 50;
        border->width.u = 50;
        border->height.u = 50;
    }
}

static void deinit_package_header(XLinkWriterContext* ctx)
{
    if (ctx && ctx->header) {
        delete[] ctx->header;
        ctx->header = nullptr;
        ctx->headerSize = 0;
    }
}

static void update_package_header(XLinkWriterContext* ctx, gsize bufferSize)
{
    auto header = reinterpret_cast<Header*>(ctx->header);
    header->package_size = ctx->headerSize + bufferSize;

    auto metaData = reinterpret_cast<Metadata*>(ctx->header + sizeof(Header));
    ++metaData->frame_number;
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
    printf("create_context\n");

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

    init_package_head(ctx);

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

    deinit_package_header(ctx);

    delete ctx;
}

static gboolean process_data(XLinkWriterContext* ctx, uint8_t* data, gsize size)
{
    update_package_header(ctx, size);

    auto handler = &ctx->handler;
    auto channelId = ctx->channelId;

    auto status = XLinkWriteData(handler, channelId, ctx->header, ctx->headerSize);
    if (status != X_LINK_SUCCESS) {
        LOG_ERROR("xlinkwriter: XLinkWriteData failed, status=%d", status);
        return FALSE;
    }

    gsize offset = 0;
    gsize fragmentSize = XLinkWriterContext::fragmentSize;

    while (offset + fragmentSize <= size) {
        auto status = XLinkWriteData(handler, channelId, data + offset, fragmentSize);
        if (status != X_LINK_SUCCESS) {
            LOG_ERROR("xlinkwriter: XLinkWriteData failed, status=%d", status);
            return FALSE;
        }
        offset += fragmentSize;
    }

    if (offset < size) {
        auto status = XLinkWriteData(handler, channelId, data + offset, size - offset);
        if (status != X_LINK_SUCCESS) {
            LOG_ERROR("xlinkwriter: XLinkWriteData failed, status=%d", status);
            return FALSE;
        }
    }

    return true;
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
