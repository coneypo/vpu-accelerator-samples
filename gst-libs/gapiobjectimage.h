/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __G_API_OBJECT_IMAGE_H__
#define __G_API_OBJECT_IMAGE_H__

#include "common.h"
#include "gapiobject.h"
#include "common.h"

G_BEGIN_DECLS

#define G_TYPE_API_OBJECT_IMAGE \
    (g_api_object_image_get_type())
#define G_API_OBJECT_IMAGE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),G_TYPE_API_OBJECT_IMAGE,GApiObjectImage))
#define G_API_OBJECT_IMAGE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),G_TYPE_API_OBJECT_IMAGE,GApiObjectImageClass))
#define G_IS_API_OBJECT_IMAGE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),G_TYPE_API_OBJECT_IMAGE))
#define G_IS_API_OBJECT_IMAGE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),G_TYPE_API_OBJECT_IMAGE))

typedef struct _GApiObjectImage      GApiObjectImage;
typedef struct _GApiObjectImageClass GApiObjectImageClass;

struct _GApiObjectImage {
    GapiObject parent;
    cv::gapi::wip::draw::Image imageInfo;
    const char *img_path;
    int alpha_val;
};

struct _GApiObjectImageClass {
    GapiObjectClass parent_class;
};

GType g_api_object_image_get_type(void);
GapiObject *gapiobjectImage_create(void);

G_END_DECLS

#endif /* __G_API_OBJECT_IMAGE_H__ */
