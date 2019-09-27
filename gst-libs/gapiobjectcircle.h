/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __G_API_OBJECT_CIRCLE_H__
#define __G_API_OBJECT_CIRCLE_H__

#include "common.h"
#include "gapiobject.h"

G_BEGIN_DECLS

#define G_TYPE_API_OBJECT_CIRCLE \
    (g_api_object_circle_get_type())
#define G_API_OBJECT_CIRCLE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),G_TYPE_API_OBJECT_CIRCLE,GApiObjectCircle))
#define G_API_OBJECT_CIRCLE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),G_TYPE_API_OBJECT_CIRCLE,GApiObjectCircleClass))
#define G_IS_API_OBJECT_CIRCLE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),G_TYPE_API_OBJECT_CIRCLE))
#define G_IS_API_OBJECT_CIRCLE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),G_TYPE_API_OBJECT_CIRCLE))

typedef struct _GApiObjectCircle      GApiObjectCircle;
typedef struct _GApiObjectCircleClass GApiObjectCircleClass;

struct _GApiObjectCircle {
    GapiObject parent;
    cv::gapi::wip::draw::Circle circleInfo;
};

struct _GApiObjectCircleClass {
    GapiObjectClass parent_class;
};

GType g_api_object_circle_get_type(void);
GapiObject *gapiobjectCircle_create(void);

G_END_DECLS

#endif /* __G_API_OBJECT_CIRCLE_H__ */
