/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __G_API_OBJECT_TEXT_H__
#define __G_API_OBJECT_TEXT_H__

#include "common.h"
#include "gapiobject.h"

G_BEGIN_DECLS

#define G_TYPE_API_OBJECT_TEXT \
    (g_api_object_text_get_type())
#define G_API_OBJECT_TEXT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),G_TYPE_API_OBJECT_TEXT,GApiObjectText))
#define G_API_OBJECT_TEXT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),G_TYPE_API_OBJECT_TEXT,GApiObjectTextClass))
#define G_IS_API_OBJECT_TEXT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),G_TYPE_API_OBJECT_TEXT))
#define G_IS_API_OBJECT_TEXT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),G_TYPE_API_OBJECT_TEXT))

typedef struct _GApiObjectText      GApiObjectText;
typedef struct _GApiObjectTextClass GApiObjectTextClass;

struct _GApiObjectText {
    GapiObject parent;
    cv::gapi::wip::draw::Text textInfo;
};

struct _GApiObjectTextClass {
    GapiObjectClass parent_class;
};

GType g_api_object_text_get_type(void);
GapiObject *gapiobjectText_create(void);

G_END_DECLS

#endif /* __G_API_OBJECT_TEXT_H__ */
