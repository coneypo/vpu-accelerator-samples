/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __G_API_OBJECT_MOSAIC_H__
#define __G_API_OBJECT_MOSAIC_H__

#include "common.h"
#include "gapiobject.h"

G_BEGIN_DECLS

#define G_TYPE_API_OBJECT_MOSAIC \
    (g_api_object_mosaic_get_type())
#define G_API_OBJECT_MOSAIC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),G_TYPE_API_OBJECT_MOSAIC,GApiObjectMosaic))
#define G_API_OBJECT_MOSAIC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),G_TYPE_API_OBJECT_MOSAIC,GApiObjectMosaicClass))
#define G_IS_API_OBJECT_MOSAIC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),G_TYPE_API_OBJECT_MOSAIC))
#define G_IS_API_OBJECT_MOSAIC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),G_TYPE_API_OBJECT_MOSAIC))

typedef struct _GApiObjectMosaic      GApiObjectMosaic;
typedef struct _GApiObjectMosaicClass GApiObjectMosaicClass;

struct _GApiObjectMosaic {
    GapiObject parent;
    cv::gapi::wip::draw::Mosaic mosaicInfo;
};

struct _GApiObjectMosaicClass {
    GapiObjectClass parent_class;
};

GType g_api_object_mosaic_get_type(void);
GapiObject *gapiobjectMosaic_create(void);

G_END_DECLS

#endif /* __G_API_OBJECT_MOSAIC_H__ */
