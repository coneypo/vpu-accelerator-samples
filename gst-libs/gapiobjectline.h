/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __G_API_OBJECT_LINEANGLE_H__
#define __G_API_OBJECT_LINEANGLE_H__

#include "common.h"
#include "gapiobject.h"

G_BEGIN_DECLS

#define G_TYPE_API_OBJECT_LINE \
    (g_api_object_line_get_type())
#define G_API_OBJECT_LINE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),G_TYPE_API_OBJECT_LINE,GApiObjectLine))
#define G_API_OBJECT_LINE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),G_TYPE_API_OBJECT_LINE,GApiObjectLineClass))
#define G_IS_API_OBJECT_LINE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),G_TYPE_API_OBJECT_LINE))
#define G_IS_API_OBJECT_LINE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),G_TYPE_API_OBJECT_LINE))

typedef struct _GApiObjectLine      GApiObjectLine;
typedef struct _GApiObjectLineClass GApiObjectLineClass;

struct _GApiObjectLine {
    GapiObject parent;
    cv::gapi::wip::draw::Line lineInfo;
};

struct _GApiObjectLineClass {
    GapiObjectClass parent_class;
};

GType g_api_object_line_get_type(void);
GapiObject *gapiobjectLine_create(void);

G_END_DECLS

#endif /* __G_API_OBJECT_LINEANGLE_H__ */
