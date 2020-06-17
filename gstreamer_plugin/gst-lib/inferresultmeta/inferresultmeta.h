/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __GST_MY_META_H__
#define __GST_MY_META_H__

#include "boundingbox.h"

#include <gst/gst.h>
#include <gst/gstmeta.h>


G_BEGIN_DECLS

typedef struct _InferResultMeta InferResultMeta;

struct _InferResultMeta {
    GstMeta meta;
    gint size;
    BoundingBox* boundingBox;
};

GType infer_result_meta_api_get_type(void);
#define INFER_RESULT_META_API_TYPE (infer_result_meta_api_get_type())

#define gst_buffer_get_infer_result_meta(b) \
    ((InferResultMeta*)gst_buffer_get_meta((b), INFER_RESULT_META_API_TYPE))

const GstMetaInfo* infer_result_meta_get_info(void);
#define INFER_RESULT_META_INFO (infer_result_meta_get_info())

InferResultMeta* gst_buffer_add_infer_result_meta(GstBuffer* buffer, gint boxNum);

G_END_DECLS
#endif
