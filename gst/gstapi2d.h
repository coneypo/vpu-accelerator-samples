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

#ifndef __GST_API_2D_H__
#define __GST_API_2D_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>
#include <json.h>
#include "gapiobject.h"

G_BEGIN_DECLS

#define GST_TYPE_API_2D \
    (gst_api_2d_get_type())
#define GST_API_2D(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_API_2D,GstApi2d))
#define GST_API_2D_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_API_2D,GstApi2dClass))
#define GST_IS_API_2D(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_API_2D))
#define GST_IS_API_2D_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_API_2D))

typedef struct _GstApi2d      GstApi2d;
typedef struct _GstApi2dClass GstApi2dClass;
typedef enum {
    BACK_END_TYPE_GAPI = 0
} BackEndType;

#define GST_API2D_TYPE_BACK_END_TYPE \
    (gst_api2d_backend_type_get_type())

GType
gst_api2d_backend_type_get_type(void) G_GNUC_CONST;

struct _GstApi2d {
    GstBaseTransform element;
    const char *config_path; //json path
    json_object *json_root; // json root object
    GList  *gapi_json_object_list; //store the objects from json config file
    GList  *gapi_buffer_object_list; //store the objects from buffer roi meta
    GstVideoInfo     sink_info;
    GstVideoInfo     src_info;
    int backend;
    GAPI_OBJECT_INFO  *object_map;
    guint  object_map_size;
    gpointer prims_pointer;
};

struct _GstApi2dClass {
    GstBaseTransformClass parent_class;
};

GType gst_api_2d_get_type(void);

G_END_DECLS

#endif /* __GST_API_2D_H__ */
