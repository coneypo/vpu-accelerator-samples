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

#ifndef __JSON_HANDLE_H__
#define __JSON_HANDLE_H__
#include <gst/gst.h>
#include <opencv2/gapi/render.hpp>
#include <json.h>

G_BEGIN_DECLS

#ifndef RETURN_VAL_IF_FAIL
#define RETURN_VAL_IF_FAIL(condition, value) \
    do{ \
        if (!(condition)) \
            return (value);  \
    }while(0)
#endif

gboolean json_get_int(struct json_object *parent, const char *name,
                      int *value);
gboolean json_get_uint(struct json_object *parent, const char *name,
                       guint *value);
const char *json_get_string(struct json_object *parent, const char *name);
gboolean json_get_double(struct json_object *parent, const char *name,
                         gdouble *value);
gboolean json_get_rgb(struct json_object *parent, const char *name,
                      cv::Scalar *color);
gboolean json_check_enable_state(struct json_object *parent,
                                 const char *enable_string);
G_END_DECLS

#endif /* __JSON_HANDLE_H__ */
