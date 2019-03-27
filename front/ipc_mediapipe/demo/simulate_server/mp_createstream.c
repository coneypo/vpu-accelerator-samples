#include "mediapipe_com.h"
#include <gst/app/app.h>
#include "XLink.h"
#include <pthread.h>

#define DATA_FRAGMENT_SIZE (1024)
static mp_int_t init_callback(mediapipe_t *mp);
static mp_int_t init_module(mediapipe_t *mp);
static gpointer read_data_from_xlink(gpointer user_data);
static void exit_master();

static XLinkHandler_t handler = {
    .devicePath = "/tmp/xlink_mock",
    .deviceType = PCIE_DEVICE
};
static XLinkGlobalHandler_t ghandler = {
    .protocol = PCIE,
};

FILE *pFile = NULL;

static mp_command_t mp_createstream_commands[] = {
    {
        mp_string("createstream"),
        MP_MAIN_CONF,
        NULL,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_module_ctx_t mp_createstream_module_ctx = {
    mp_string("createstream"),
    NULL,
    NULL,
    NULL
};

mp_module_t mp_createstream_module = {
    MP_MODULE_V1,
    &mp_createstream_module_ctx,       /* module context */
    mp_createstream_commands,          /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                              /* init master */
    init_module,                              /* init module */
    NULL,                              /* keyshot_process*/
    NULL,                              /* message_process */
    init_callback,                     /* init_callback */
    NULL,                              /* netcommand_process */
    exit_master,                              /* exit master */
    MP_MODULE_V1_PADDING
};

typedef struct {
    /*header*/
    guint8  magic;
    guint8  version;
    guint16 meta_size;
    guint32 package_size;
} packageHeader;

typedef struct {
    /*Metadata*/
    guint8  version;
    guint8  packet_type;
    guint8  stream_id;
    guint8  of_objects;
    guint32 frame_number;
} packageMetadata;

#pragma pack(push)
#pragma pack(1)
typedef struct {
    unsigned u : 24;
} Foo;
#pragma pack(pop)

typedef struct {
    /*object*/
    guint8  reserved;
    guint8  object_id;
    guint16 classfication_GT;
    Foo left;
    Foo top;
    Foo width;
    Foo height;
} objectBorder;

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis fill info for buffer header
 *
 * @Param header
 * @Param metadata
 * @Param border
 * @Param bufferSize
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
static gboolean get_test_packet(packageHeader *header,
                                packageMetadata *metadata, objectBorder **border, gsize bufferSize)
{
    if (!header || !metadata || !border || !bufferSize || *border) {
        g_print("[ERROR]: createstream param error!");
        return FALSE;
    }
    static guint32 frame = 0;
    /*fill in the metadata*/
    metadata->version = 1;
    metadata->packet_type = 1;
    metadata->stream_id = 1;
    metadata->of_objects = 1;
    metadata->frame_number = frame;
    frame++;
    /* fill in the header */
    header->magic = 1;
    header->version = 1;
    header->meta_size = sizeof(packageMetadata) + metadata->of_objects * sizeof(
                            objectBorder);
    header->package_size = header->meta_size + bufferSize + sizeof(packageHeader);
    /*malloc objects and fill in it*/
    *border = (objectBorder *)malloc(metadata->of_objects * sizeof(objectBorder));
    memset(*border, 1, metadata->of_objects * sizeof(objectBorder));
    (*border)->left.u = 50;
    (*border)->top.u = 50;
    (*border)->width.u = 50;
    (*border)->height.u = 50;
    return TRUE;
}


static mp_int_t
init_module(mediapipe_t *mp)
{
    while (XLinkInitialize(&ghandler) != X_LINK_SUCCESS) {
        LOG_ERROR("can't init xlink");
    }
    while (XLinkBootRemote(&handler, DEFAULT_NOMINAL_MAX) != X_LINK_SUCCESS) {
        LOG_ERROR("can't boot remote");
    }
    while (XLinkConnect(&handler) != X_LINK_SUCCESS) {
        LOG_ERROR("can't xlink connect");
    }
    OperationMode_t operationType = RXB_TXB;
    channelId_t echoChannel = 0x400;
    //XLinkOpenStream stubbed for PCIe
    while (XLinkOpenChannel(&handler, echoChannel, operationType,
                            DATA_FRAGMENT_SIZE, 0) != X_LINK_SUCCESS) {
        LOG_ERROR("can't open channel");
    }
    //open file
    pFile = fopen("./streamFile.h264", "ab");
    if (!pFile) {
        LOG_ERROR("createstream: open file error!");
        return MP_ERROR;
    }
    g_thread_new("read_data", read_data_from_xlink, NULL);
    return MP_OK;
}

static gpointer
read_data_from_xlink (gpointer user_data)
{
    while(1){

        int streamId, status;
        uint8_t* message = NULL;
        uint32_t msg_size = 0;
        channelId_t echoChannel = 0x400;
        status =  XLinkReadData(&handler, echoChannel, &message, &msg_size);
        if (status != X_LINK_SUCCESS){
            LOG_ERROR("Failed to read the message from the device.");
            return NULL;
        }
        LOG_DEBUG("%d", msg_size);
        status = XLinkReleaseData(&handler, echoChannel, message);
        if (status != X_LINK_SUCCESS) {
            LOG_ERROR("XLinkWriteData ARM release data failed: %x\n", status);
            return NULL;
        }
    }

    return NULL;
}

static gboolean
proc_src_callback(mediapipe_t *mp, GstBuffer *buf, guint8 *data, gsize size,
                  gpointer user_data)
{
    GstMapInfo info;
    if (!gst_buffer_map(buf, &info, GST_MAP_READ)) {
        LOG_ERROR("createstream: map buffer error!");
        return FALSE;
    }
    packageHeader header;
    packageMetadata metadata;
    objectBorder *pBorder = NULL;
    channelId_t echoChannel = 0x400;
    gsize bufferSize = gst_buffer_get_size(buf);
    if (!get_test_packet(&header, &metadata, &pBorder, bufferSize)) {
        LOG_ERROR("createstream: get_test_packet() error!");
        gst_buffer_unmap(buf, &info);
        return FALSE;
    }
    //write to file
    fwrite(&header , sizeof(header), 1, pFile);
    fwrite(&metadata , sizeof(metadata), 1, pFile);
    fwrite(pBorder , sizeof(objectBorder) * metadata.of_objects, 1, pFile);
    fwrite(info.data , sizeof(char) * bufferSize, 1, pFile);
    //writ to xlink
    int streamId, status;
    status =  XLinkWriteData(&handler, echoChannel, (uint8_t *)&header,
                             sizeof(header));
    if (status != X_LINK_SUCCESS) {
        LOG_ERROR("Failed to write the message to the device.");
        return FALSE;
    }
    status =  XLinkWriteData(&handler, echoChannel, (uint8_t *)&metadata,
                             sizeof(metadata));
    if (status != X_LINK_SUCCESS) {
        LOG_ERROR("Failed to write the message to the device.");
        return FALSE;
    }
    status =  XLinkWriteData(&handler, echoChannel, (uint8_t *)pBorder,
                             sizeof(objectBorder) * metadata.of_objects);
    if (status != X_LINK_SUCCESS) {
        LOG_ERROR("Failed to write the message to the device.");
        return FALSE;
    }
    guint write_offset = 0;
    gpointer pic = info.data;
    while (write_offset < bufferSize) {
        /* g_print("offset:%d\n", write_offset); */
        /* g_print("bufferSize:%ld\n", bufferSize); */
        if (write_offset + DATA_FRAGMENT_SIZE <= bufferSize) {
            status =  XLinkWriteData(&handler, echoChannel, (uint8_t *)pic + write_offset,
                                     DATA_FRAGMENT_SIZE);
        } else {
            status =  XLinkWriteData(&handler, echoChannel, (uint8_t *)pic + write_offset,
                                     bufferSize - write_offset);
        }
        if (status != X_LINK_SUCCESS) {
            LOG_ERROR("Failed to write the message to the device.");
            return FALSE;
        }
        write_offset += DATA_FRAGMENT_SIZE;
    }
    //write end
    gst_buffer_unmap(buf, &info);
    free(pBorder);
    return TRUE;
}

static void
exit_master()
{
    if (pFile) {
        fclose(pFile);
    }
    XLinkCloseChannel(&handler, 0x400);
    return;
}

static mp_int_t
init_callback(mediapipe_t *mp)
{
    mediapipe_set_user_callback(mp, "proc_src", "src", proc_src_callback, NULL);
    return MP_OK;
}

