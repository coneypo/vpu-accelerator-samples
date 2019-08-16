/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) YEAR AUTHOR_NAME AUTHOR_EMAIL
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
