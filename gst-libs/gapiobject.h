/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __G_API_OBJECT_H__
#define __G_API_OBJECT_H__

#include <gst/video/video.h>
#include <json.h>

G_BEGIN_DECLS

#define G_TYPE_API_OJBECT \
    (g_api_object_get_type())
#define G_API_OBJECT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),G_TYPE_API_OJBECT,GapiObject))
#define G_API_OBJECT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),G_TYPE_API_OJBECT,GapiObjectClass))
#define G_IS_API_OBJECT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),G_TYPE_API_OJBECT))
#define G_IS_API_OBJECT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),G_TYPE_API_OJBECT))
#define G_API_OBJECT_TO_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS(obj, G_TYPE_API_OJBECT, GapiObjectClass))
#define UNUSED(x) ((void)(x))

typedef struct _GapiObject      GapiObject;
typedef struct _GapiObjectClass GapiObjectClass;

struct _GapiObject {
    GObject parent;
    guint   meta_id;  // uniqu in a list
    const char *meta_type; // for exampe： text, pic, mask
};

struct _GapiObjectClass {
    GObjectClass parent_class;
    gboolean(*parse_json)(GapiObject *object, json_object *json_object);
    gboolean(*parse_gst_structure)(GapiObject *object, GstStructure *strut);
    GstStructure *(*to_gst_structure)(GapiObject *object);
    //it's used for do operations together
    void (*render_submit)(GapiObject *object, gpointer prims_pointer);
    //it's used for do operations one by one
    gboolean(*render_ip)(GapiObject *object, GstVideoInfo *sink_info,
                         GstVideoInfo *src_info, GstBuffer *buf);
    //it's used for video scale,video crop, video format convert, or just don't want to modify the origin buffer
    gboolean(*render)(GapiObject *object, GstBuffer *in_buf, GstBuffer *out_buf);
};
//typedef enum {
//    GAPI_BACKEND_TYPE_CPU,
//    GAPI_BACKEND_TYPE_GPU
//}GAPI_BACKEND_TYPE

GType g_api_object_get_type(void);

typedef GapiObject *(*create_func)();

typedef struct  {
    const char *object_type;
    GType type;
    create_func create;
} GAPI_OBJECT_INFO;


gboolean render_sync(GstBuffer *outbuf, GstVideoInfo *sink_info,
                     GstVideoInfo *src_info, gpointer prims_pointer);

gpointer init_array();
void destory_array(gpointer prims_pointer);
extern GAPI_OBJECT_INFO gapi_info_map[];
extern const int gapi_info_map_size;

G_END_DECLS

#endif /* __G_API_OBJECT_H__ */
