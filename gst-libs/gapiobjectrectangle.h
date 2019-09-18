/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __G_API_OBJECT_RECTANGLE_H__
#define __G_API_OBJECT_RECTANGLE_H__

#include "common.h"
#include "gapiobject.h"

G_BEGIN_DECLS

#define G_TYPE_API_OBJECT_RECT \
    (g_api_object_rect_get_type())
#define G_API_OBJECT_RECT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),G_TYPE_API_OBJECT_RECT,GApiObjectRect))
#define G_API_OBJECT_RECT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),G_TYPE_API_OBJECT_RECT,GApiObjectRectClass))
#define G_IS_API_OBJECT_RECT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),G_TYPE_API_OBJECT_RECT))
#define G_IS_API_OBJECT_RECT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),G_TYPE_API_OBJECT_RECT))

typedef struct _GApiObjectRect      GApiObjectRect;
typedef struct _GApiObjectRectClass GApiObjectRectClass;

struct _GApiObjectRect {
    GapiObject parent;
    cv::gapi::wip::draw::Rect rectInfo;
};

struct _GApiObjectRectClass {
    GapiObjectClass parent_class;
};

GType g_api_object_rect_get_type(void);
GapiObject *gapiobjectRect_create(void);

G_END_DECLS

#endif /* __G_API_OBJECT_RECTANGLE_H__ */
