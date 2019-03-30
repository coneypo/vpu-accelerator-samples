#include <gst/app/app.h>

#include "XLink.h"
#include "mediapipe_com.h"

static mp_int_t init_callback(mediapipe_t* mp);

static mp_command_t mp_xlinkreader_commands[] = {
    mp_custom_command0("xlinkreader"),
    mp_null_command
};

static mp_module_ctx_t mp_xlinkreader_module_ctx = {
    mp_string("xlinkreader"),
    nullptr,
    nullptr,
	nullptr
};

mp_module_t mp_xlinkreader_module = {
    MP_MODULE_V1,
    &mp_xlinkreader_module_ctx,    /* module context */
    mp_xlinkreader_commands,       /* module directives */
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
/*
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
}
*/

static gboolean
xlinksrc_src_callback(mediapipe_t* mp, GstBuffer* buffer, guint8* data, gsize size, gpointer user_data)
{
	GstMapInfo info;
	if (!gst_buffer_map(buffer, &info, GST_MAP_READ)) {
		LOG_ERROR("xlinkreader: gst_buffer_map failed");
		return FALSE;
	}

	auto pData = info.data;
	auto header = reinterpret_cast<Header*>(pData);
	auto metaData = reinterpret_cast<Metadata*>(pData + sizeof(Header));
	auto border = reinterpret_cast<ObjectBorder*>(pData + sizeof(Header) + sizeof(Metadata));

	auto frameId = metaData->frame_number;
	auto headerSize = header->meta_size + sizeof(header);

	for (int i = 0; i < metaData->of_objects; i++) {
        GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(
            buffer, "label", border[i].left.u, border[i].top.u, border[i].width.u, border[i].height.u);
        GstStructure *s = gst_structure_new(
            "detection", "reserved", G_TYPE_UINT, border[i].reserved, "object_id", G_TYPE_UINT, border[i].object_id,
            "classfication_GT", G_TYPE_UINT, border[i].classfication_GT, "left", G_TYPE_UINT, border[i].left.u,
            "top", G_TYPE_UINT, border[i].top.u, "width", G_TYPE_UINT, border[i].width.u, "height", G_TYPE_UINT, border[i].height.u, NULL);
        gst_video_region_of_interest_meta_add_param(meta, s);
	}

	gst_buffer_unmap(buffer, &info);

    gst_buffer_resize(buffer, headerSize, -1);

#ifndef DEBUG
    gst_buffer_map(buffer, &info, GST_MAP_READ);
    pData = info.data;
    LOG_INFO("[id:%u] %02X %02X %02X %02X", frameId, pData[64], pData[128], pData[256], pData[512]);
    gst_buffer_unmap(buffer, &info);
#endif

	return TRUE;
}

static mp_int_t init_callback(mediapipe_t* mp)
{
    mediapipe_set_user_callback(mp, "src", "src", xlinksrc_src_callback, NULL);

    return MP_OK;
}
