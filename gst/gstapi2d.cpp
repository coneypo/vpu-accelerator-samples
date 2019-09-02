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

/**
 * SECTION:element-plugin
 *
 * FIXME:Describe plugin here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! plugin ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/controller/controller.h>
#include <string>
#include "gstapi2d.h"

GST_DEBUG_CATEGORY_STATIC(gst_api_2d_debug);
#define GST_CAT_DEFAULT gst_api_2d_debug

/* Filter signals and args */
enum {
    /* FILL ME */
    LAST_SIGNAL
};

enum {
    PROP_0 = 0,
    PROP_CONFIG_PATH,
    PROP_CONFIG_LIST,
    PROP_BACK_END,
    PROP_MAX,
    PROP_DRAW_ROI
};

#define COMMON_VIDEO_CAPS \
    "width = (int) [ 16, 4096 ], " \
    "height = (int) [ 16, 4096 ] "

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template =
    GST_STATIC_PAD_TEMPLATE(
        "sink",
        GST_PAD_SINK,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS("video/x-raw, format = (string) {NV12, BGR},  "
                        COMMON_VIDEO_CAPS)
    );

static GstStaticPadTemplate src_template =
    GST_STATIC_PAD_TEMPLATE(
        "src",
        GST_PAD_SRC,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS("video/x-raw, format = (string) {NV12, BGR},  "
                        COMMON_VIDEO_CAPS)
    );

#define gst_api_2d_parent_class parent_class
G_DEFINE_TYPE(GstApi2d, gst_api_2d, GST_TYPE_BASE_TRANSFORM);

static void gst_api_2d_set_property(GObject *object, guint prop_id,
                                    const GValue *value, GParamSpec *pspec);
static void gst_api_2d_get_property(GObject *object, guint prop_id,
                                    GValue *value, GParamSpec *pspec);

static GAPI_OBJECT_INFO *find_info_by_type(GstApi2d *filter, const char *item_type);

static gboolean parse_from_json_file(GstApi2d *filter);

static GList* parse_gst_structure_list(GstApi2d *filter, GList* structure_list);

static GList* get_structure_list_from_object_list(GList *object_list);

static const char *get_type_from_json(GstApi2d *filter, json_object *item);

static const gchar *get_type_from_gst_struture(GstApi2d *filter, GstStructure *struct_item);

static GstFlowReturn gst_api_2d_transform_ip(GstBaseTransform *base,
        GstBuffer *outbuf);

static void
gst_api_2d_finalize(GObject *object);

static gboolean
gst_api_2d_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
/* GObject vmethod implementations */

/* initialize the plugin's class */
static void
gst_api_2d_class_init(GstApi2dClass *klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    GstBaseTransformClass *trans_class = (GstBaseTransformClass *) klass;
    gobject_class->set_property = gst_api_2d_set_property;
    gobject_class->get_property = gst_api_2d_get_property;
    gobject_class->finalize = gst_api_2d_finalize;
    trans_class->set_caps = gst_api_2d_set_caps;
    g_object_class_install_property(gobject_class, PROP_CONFIG_PATH,
                                    g_param_spec_string("config-path", "config-path", "json configure path ",
                                            NULL,  GParamFlags(G_PARAM_READWRITE | G_PARAM_CONSTRUCT)));
    g_object_class_install_property(gobject_class, PROP_CONFIG_LIST,
                                    g_param_spec_pointer("config-list", "config-list", "GstStructure GList ",
                                            G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_BACK_END,
                                    g_param_spec_enum("backend", "backend", "backend", GST_API2D_TYPE_BACK_END_TYPE,
                                            0,
                                            G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_DRAW_ROI,
                                    g_param_spec_boolean("drawroi", "drawroi", "drawroi", false,
                                            G_PARAM_READWRITE));
    gst_element_class_set_details_simple(gstelement_class,
                                         "Plugin",
                                         "Generic/Filter",
                                         "FIXME:Generic 2d Filter",
                                         "hu, yuan yuanhu2@intel.com");
    gst_element_class_add_pad_template(gstelement_class,
                                       gst_static_pad_template_get(&src_template));
    gst_element_class_add_pad_template(gstelement_class,
                                       gst_static_pad_template_get(&sink_template));
    GST_BASE_TRANSFORM_CLASS(klass)->transform_ip =
        GST_DEBUG_FUNCPTR(gst_api_2d_transform_ip);
}
/* initialize the new element
 * initialize instance structure
 */
static void
gst_api_2d_init(GstApi2d *filter)
{
    filter->config_path = NULL;
    filter->json_root = NULL; // json root object
    filter->gapi_json_object_list = NULL; //store the objects from json config file
    filter->gapi_configure_structure_list = NULL; //store the structures from configure structure list
    filter->gapi_buffer_object_list = NULL; //store the objects from buffer roi meta
    gst_video_info_init(&filter->sink_info);
    gst_video_info_init(&filter->src_info);
    filter->object_map = gapi_info_map;
    filter->object_map_size = gapi_info_map_size;
    filter->prims_pointer = init_array();
    g_rw_lock_init(&filter->rwlock);
    filter->drawroi = false;
}

static void
gst_api_2d_set_property(GObject *object, guint prop_id,
                        const GValue *value, GParamSpec *pspec)
{
    g_assert(object != NULL);
    GstApi2d *filter = GST_API_2D(object);
    GList *temp = NULL;
    switch (prop_id) {
        case PROP_CONFIG_PATH:
            filter->config_path = g_strdup(g_value_get_string(value));
            g_rw_lock_writer_lock(&filter->rwlock);
            g_list_free_full(filter->gapi_json_object_list, (GDestroyNotify) g_object_unref);
            parse_from_json_file(filter);
            g_rw_lock_writer_unlock(&filter->rwlock);
            break;
        case PROP_CONFIG_LIST:
            temp = parse_gst_structure_list(filter, (GList *)g_value_get_pointer(value));
            g_rw_lock_writer_lock(&filter->rwlock);
            g_list_free_full(filter->gapi_json_object_list, (GDestroyNotify) g_object_unref);
            filter->gapi_json_object_list = temp;
            g_rw_lock_writer_unlock(&filter->rwlock);
            break;
        case PROP_BACK_END:
            filter->backend = g_value_get_enum(value);
            break;
        case PROP_DRAW_ROI:
            filter->drawroi = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_api_2d_get_property(GObject *object, guint prop_id,
                        GValue *value, GParamSpec *pspec)
{
    g_assert(object != NULL);
    g_assert(value != NULL);
    GstApi2d *filter = GST_API_2D(object);
    switch (prop_id) {
        case PROP_CONFIG_PATH:
            g_value_set_string(value, filter->config_path);
            break;
        case PROP_CONFIG_LIST:
            if (filter->gapi_configure_structure_list != NULL) {
                g_list_free_full(filter->gapi_configure_structure_list, (GDestroyNotify)gst_structure_free);
                filter->gapi_configure_structure_list = NULL;
            }
            g_rw_lock_reader_lock(&filter->rwlock);
            filter->gapi_configure_structure_list = get_structure_list_from_object_list(filter->gapi_json_object_list);
            g_rw_lock_reader_unlock(&filter->rwlock);
            g_value_set_pointer(value, filter->gapi_configure_structure_list);
            break;
        case PROP_BACK_END:
            g_value_set_enum(value, filter->backend);
            break;
        case PROP_DRAW_ROI:
            g_value_set_boolean(value, filter->drawroi);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

/* GstBaseTransform vmethod implementations */

/* this function does the actual processing
 */
static GstFlowReturn
gst_api_2d_transform_ip(GstBaseTransform *base, GstBuffer *outbuf)
{
    GstApi2d *filter = GST_API_2D(base);
    g_rw_lock_reader_lock(&filter->rwlock);
    GList *list = filter->gapi_json_object_list;
    while (list != NULL) {
        GapiObject *object = (GapiObject *) list->data;
        GapiObjectClass *objectclass = G_API_OBJECT_TO_CLASS(object);
        objectclass->render_submit(object, filter->prims_pointer);
        list = list->next;
    }
    g_rw_lock_reader_unlock(&filter->rwlock);
    render_sync(outbuf, &filter->sink_info, &filter->src_info, filter->prims_pointer);
    return GST_FLOW_OK;
}

static void
gst_api_2d_finalize(GObject *object)
{
    g_assert(object != NULL);
    GstApi2d *filter = GST_API_2D(object);
    if (filter->json_root) {
        json_object_put(filter->json_root);
    }
    g_free ((gpointer)filter->config_path);
    g_list_free_full(filter->gapi_json_object_list, (GDestroyNotify) g_object_unref);
    g_list_free_full(filter->gapi_buffer_object_list, (GDestroyNotify) g_object_unref);
    g_list_free_full(filter->gapi_configure_structure_list, (GDestroyNotify)gst_structure_free);
    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    destory_array(filter->prims_pointer);
    g_rw_lock_clear(&filter->rwlock);
    G_OBJECT_CLASS(parent_class)->finalize(object);
}

GType
gst_api2d_backend_type_get_type(void)
{
    static volatile GType g_type;
    static const GEnumValue backend_type[] = {
        {BACK_END_TYPE_GAPI, "GAPI", "gapi" },
        {0, NULL, NULL }
    };
    if (g_once_init_enter(&g_type)) {
        GType type = g_enum_register_static("GstApi2dBackEndType", backend_type);
        GST_INFO("Registering Gst Api2d BackEnd Type");
        g_once_init_leave(&g_type, type);
    }
    return g_type;
}

static gboolean parse_from_json_file(GstApi2d *filter)
{
    if (filter->config_path == NULL) {
        GST_WARNING_OBJECT(filter, "json file path is null");
        return FALSE ;
    } else{
        GST_WARNING_OBJECT(filter, "json file path is %s", filter->config_path);
    }
    if (filter->json_root != NULL) {
        return FALSE;
    }
    if (filter->gapi_json_object_list != NULL) {
        return FALSE;
    }
    filter->json_root = json_object_from_file(filter->config_path);
    if (filter->json_root == NULL) {
        GST_WARNING_OBJECT(filter, "json file syntax error");
        return FALSE;
    }
    int length = json_object_array_length(filter->json_root);
    for (int i = 0; i < length; i++) {
        json_object *item = json_object_array_get_idx(filter->json_root, i);
        const char *item_type = get_type_from_json(filter, item);
        GAPI_OBJECT_INFO  *info = find_info_by_type(filter, item_type);
        if (info) {
            GapiObject *object = info->create();
            GapiObjectClass *objectclass = G_API_OBJECT_TO_CLASS(object);
            objectclass->parse_json(object, item);
            filter->gapi_json_object_list = g_list_append(filter->gapi_json_object_list,
                                            object);
        }
    }
    return TRUE;
}

static GList* parse_gst_structure_list(GstApi2d *filter, GList* structure_list)
{
    g_assert(filter != NULL);

    if (structure_list == NULL) {
        GST_WARNING_OBJECT(filter, "GstStructure_list is null");
        return NULL ;
    }

    GstStructure *structure = NULL;
    GList *object_list = NULL;
    GList *index = structure_list;
    while(index) {
        structure = (GstStructure *)index->data;
        const char* item_type = get_type_from_gst_struture(filter, structure);
        GAPI_OBJECT_INFO *info = find_info_by_type(filter, item_type);
        if (info) {
            GapiObject *object = info->create();
            GapiObjectClass *objectclass = G_API_OBJECT_TO_CLASS(object);
            if (objectclass->parse_gst_structure(object, structure)) {
                object_list = g_list_append(object_list, object);
            }
        }
        index = g_list_next(index);
    }
    return object_list;
}

static GList *get_structure_list_from_object_list(GList *object_list)
{
    if (NULL == object_list) {
        return NULL;
    }
    GstStructure *structure = NULL;
    GapiObject *object = NULL;
    GList *index = object_list;
    GList * structure_list = NULL;
    while (index) {
        object = (GapiObject *)index->data;
        GapiObjectClass *objectclass = G_API_OBJECT_TO_CLASS(object);
        if (structure = objectclass->to_gst_structure(object)) {
            structure_list= g_list_append(structure_list, structure);
        }
        index = g_list_next(index);
    }
    return structure_list;
}

static const char *get_type_from_json(GstApi2d *filter, json_object *item)
{
    g_assert(filter != NULL);
    g_assert(item != NULL);
    json_object *tmp = NULL;
    if (json_object_object_get_ex(item, "meta_type", &tmp)) {
        const char *item_type = json_object_get_string(tmp);
        return item_type;
    }
    return NULL;
}

static const gchar *get_type_from_gst_struture(GstApi2d *filter, GstStructure *struct_item)
{
    g_assert(filter != NULL);
    g_assert(struct_item != NULL);
    if(gst_structure_has_field(struct_item, "meta_type")) {
        const gchar *item_type = gst_structure_get_string(struct_item, "meta_type");
        return item_type;
    }
    return NULL;
}

static GAPI_OBJECT_INFO *
find_info_by_type(GstApi2d *filter, const char *item_type)
{
    g_assert(filter != NULL);
    g_assert(item_type != NULL);
    for (guint i = 0; i < filter->object_map_size; i++) {
        if (std::string(item_type) == std::string(filter->object_map[i].object_type)) {
            return &filter->object_map[i];
        }
    }
    GST_WARNING_OBJECT(filter, "can't find element info about:%s", item_type);
    return NULL;
}

static gboolean
gst_api_2d_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps)
{
    g_assert(trans != NULL);
    g_assert(incaps != NULL);
    g_assert(outcaps != NULL);
    GstApi2d *filter = GST_API_2D(trans);
    if (!gst_video_info_from_caps(&filter->sink_info, incaps) ||
        !gst_video_info_from_caps(&filter->src_info, outcaps)) {
        GST_ERROR_OBJECT(filter, "invalid caps");
        return FALSE;
    }
    return TRUE;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
plugin_init(GstPlugin *plugin)
{
    /* debug category for fltering log messages
     *
     * FIXME:exchange the string 'Template plugin' with your description
     */
    GST_DEBUG_CATEGORY_INIT(gst_api_2d_debug, "api2d", 0, "api2d plugin");
    return gst_element_register(plugin, "api2d", GST_RANK_NONE,
                                GST_TYPE_API_2D);
}

/* gstreamer looks for this structure to register plugins
 *
 * FIXME:exchange the string 'Template plugin' with you plugin description
 */
GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    api2d,
    "api2d plugin",
    plugin_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)

