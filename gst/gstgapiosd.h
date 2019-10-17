/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __GST_GAPI_OSD_H__
#define __GST_GAPI_OSD_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>
#include <json.h>
#include "gapiobject.h"

G_BEGIN_DECLS

#define GST_TYPE_GAPI_OSD \
    (gst_gapi_osd_get_type())
#define GST_GAPI_OSD(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GAPI_OSD,GstGapiosd))
#define GST_GAPI_OSD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GAPI_OSD,GstGapiosdClass))
#define GST_IS_GAPI_OSD(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GAPI_OSD))
#define GST_IS_GAPI_OSD_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GAPI_OSD))

typedef struct _GstGapiosd      GstGapiosd;
typedef struct _GstGapiosdClass GstGapiosdClass;
typedef enum {
    BACK_END_TYPE_GAPI = 0
} BackEndType;

#define GST_GAPIOSD_TYPE_BACK_END_TYPE \
    (gst_gapiosd_backend_type_get_type())

GType
gst_gapiosd_backend_type_get_type(void) G_GNUC_CONST;

struct _GstGapiosd {
    GstBaseTransform element;
    const char *config_path; //json path
    json_object *json_root; // json root object
    GList  *gapi_json_object_list; //store the objects from json config file
    GList  *gapi_configure_structure_list; //store the structures from configure structure list
    GList  *gapi_buffer_object_list; //store the objects from buffer roi meta
    GstVideoInfo     sink_info;
    GstVideoInfo     src_info;
    int backend;
    GAPI_OBJECT_INFO  *object_map;
    guint  object_map_size;
    gpointer prims_pointer;
    GRWLock rwlock;
    gboolean drawroi;
    gboolean is_dma;
    gchar  *allocator_name;
};

struct _GstGapiosdClass {
    GstBaseTransformClass parent_class;
};

GType gst_gapi_osd_get_type(void);

G_END_DECLS

#endif /* __GST_GAPI_OSD_H__ */
