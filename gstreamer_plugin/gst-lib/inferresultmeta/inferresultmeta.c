#include "inferresultmeta.h"
#include <string.h>

#define MAX_STR_LEN 100

GType infer_result_meta_api_get_type(void)
{
    static volatile GType type;
    static const gchar* tags[] = { NULL };

    if (g_once_init_enter(&type)) {
        GType _type = gst_meta_api_type_register("InferResultMetaAPI", tags);
        g_once_init_leave(&type, _type);
    }
    return type;
}

static gboolean
infer_result_meta_init(GstMeta* meta, gpointer params, GstBuffer* buffer)
{
    InferResultMeta* emeta = (InferResultMeta*)meta;

    emeta->size = *(gint*)params;
    emeta->boundingBox = (BoundingBox*)g_malloc0(sizeof(BoundingBox) * emeta->size);
    for (gint i = 0; i < emeta->size; i++) {
        emeta->boundingBox[i].label = g_malloc(MAX_STR_LEN);
    }

    return TRUE;
}

static gboolean
infer_result_meta_transform(GstBuffer* transbuf, GstMeta* meta,
    GstBuffer* buffer, GQuark type, gpointer data)
{
    InferResultMeta* emeta = (InferResultMeta*)meta;

    /* we always copy no matter what transform */
    InferResultMeta* new_meta = gst_buffer_add_infer_result_meta(transbuf, emeta->size);

    memcpy(new_meta->boundingBox, emeta->boundingBox, sizeof(BoundingBox) * emeta->size);
    for (gint i = 0; i < emeta->size; i++) {
        emeta->boundingBox[i].label = NULL;
    }

    return TRUE;
}

static void
infer_result_meta_free(GstMeta* meta, GstBuffer* buffer)
{
    InferResultMeta* emeta = (InferResultMeta*)meta;

    for (gint i = 0; i < emeta->size; i++) {
        g_free(emeta->boundingBox[i].label);
        emeta->boundingBox[i].label = NULL;
    }

    g_free(emeta->boundingBox);
    emeta->boundingBox = NULL;
}

const GstMetaInfo*
infer_result_meta_get_info(void)
{
    static const GstMetaInfo* meta_info = NULL;

    if (g_once_init_enter(&meta_info)) {
        const GstMetaInfo* mi = gst_meta_register(INFER_RESULT_META_API_TYPE,
            "InferResultMeta",
            sizeof(InferResultMeta),
            infer_result_meta_init,
            infer_result_meta_free,
            infer_result_meta_transform);
        g_once_init_leave(&meta_info, mi);
    }
    return meta_info;
}

InferResultMeta*
gst_buffer_add_infer_result_meta(GstBuffer* buffer, gint boxNum)
{
    InferResultMeta* meta;

    g_return_val_if_fail(GST_IS_BUFFER(buffer), NULL);

    meta = (InferResultMeta*)gst_buffer_add_meta(buffer,
        INFER_RESULT_META_INFO, &boxNum);

    return meta;
}
