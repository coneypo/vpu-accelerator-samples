/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
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
#include "gstgapiosd.h"

GST_DEBUG_CATEGORY_STATIC(gst_gapi_osd_debug);
#define GST_CAT_DEFAULT gst_gapi_osd_debug
#define DEFAULT_ALLOCATOR_NAME ""

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
    PROP_DRAW_ROI,
    PROP_ALLOCATOR_NAME
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
        GST_STATIC_CAPS("video/x-raw, format = (string) {NV12, BGR}," \
                        COMMON_VIDEO_CAPS ";"  \
                        "video/x-raw(memory:DMABuf), format = (string) {NV12, BGR}," \
                        COMMON_VIDEO_CAPS
                        )

    );

static GstStaticPadTemplate src_template =
    GST_STATIC_PAD_TEMPLATE(
        "src",
        GST_PAD_SRC,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS("video/x-raw, format = (string) {NV12, BGR}," \
                        COMMON_VIDEO_CAPS ";"  \
                        "video/x-raw(memory:DMABuf), format = (string) {NV12, BGR}," \
                        COMMON_VIDEO_CAPS
                        )
    );

#define gst_gapi_osd_parent_class parent_class
G_DEFINE_TYPE(GstGapiosd, gst_gapi_osd, GST_TYPE_BASE_TRANSFORM);

static void gst_gapi_osd_set_property(GObject *object, guint prop_id,
                                    const GValue *value, GParamSpec *pspec);
static void gst_gapi_osd_get_property(GObject *object, guint prop_id,
                                    GValue *value, GParamSpec *pspec);

static GAPI_OBJECT_INFO *find_info_by_type(GstGapiosd *filter, const char *item_type);

static gboolean parse_from_json_file(GstGapiosd *filter);

static GList* parse_gst_structure_list(GstGapiosd *filter, GList* structure_list);

static GList* get_structure_list_from_object_list(GList *object_list);

static const char *get_type_from_json(GstGapiosd *filter, json_object *item);

static const gchar *get_type_from_gst_struture(GstGapiosd *filter, GstStructure *struct_item);

static GList * parse_gststructure_from_roimeta(GstGapiosd *filter, GstBuffer *buffer);

static GstFlowReturn gst_gapi_osd_transform_ip(GstBaseTransform *base, GstBuffer *buf);

static gboolean
gst_gapi_osd_decide_allocation(GstBaseTransform *trans, GstQuery *query);

static void
gst_gapi_osd_finalize(GObject *object);

static gboolean
gst_gapi_osd_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
/* GObject vmethod implementations */

/* initialize the plugin's class */
static void
gst_gapi_osd_class_init(GstGapiosdClass *klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    GstBaseTransformClass *trans_class = (GstBaseTransformClass *) klass;
    gobject_class->set_property = gst_gapi_osd_set_property;
    gobject_class->get_property = gst_gapi_osd_get_property;
    gobject_class->finalize = gst_gapi_osd_finalize;
    trans_class->set_caps = gst_gapi_osd_set_caps;
    trans_class->decide_allocation = gst_gapi_osd_decide_allocation;
    g_object_class_install_property(gobject_class, PROP_CONFIG_PATH,
                                    g_param_spec_string("config-path", "config-path", "json configure path ",
                                            NULL,  GParamFlags(G_PARAM_READWRITE | G_PARAM_CONSTRUCT)));
    g_object_class_install_property(gobject_class, PROP_CONFIG_LIST,
                                    g_param_spec_pointer("config-list", "config-list", "GstStructure GList ",
                                            G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_BACK_END,
                                    g_param_spec_enum("backend", "backend", "backend", GST_GAPIOSD_TYPE_BACK_END_TYPE,
                                            0,
                                            G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_DRAW_ROI,
                                    g_param_spec_boolean("drawroi", "drawroi", "drawroi", false,
                                            G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_ALLOCATOR_NAME,
                                    g_param_spec_string("allocator-name", "AllocatorName",
                                                        "Registered allocator name to be used", DEFAULT_ALLOCATOR_NAME,
                                                         GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
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
        GST_DEBUG_FUNCPTR(gst_gapi_osd_transform_ip);
}
/* initialize the new element
 * initialize instance structure
 */
static void
gst_gapi_osd_init(GstGapiosd *filter)
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
    filter->is_dma = false;
}

static void
gst_gapi_osd_set_property(GObject *object, guint prop_id,
                        const GValue *value, GParamSpec *pspec)
{
    g_assert(object != NULL);
    GstGapiosd *filter = GST_GAPI_OSD(object);
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
        case PROP_ALLOCATOR_NAME:
            filter->allocator_name = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_gapi_osd_get_property(GObject *object, guint prop_id,
                        GValue *value, GParamSpec *pspec)
{
    g_assert(object != NULL);
    g_assert(value != NULL);
    GstGapiosd *filter = GST_GAPI_OSD(object);
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
        case PROP_ALLOCATOR_NAME:
            g_value_set_string(value, filter->allocator_name);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

/* GstBaseTransform vmethod implementations */

/* this function does the actual processing
 */
#define ALIGN(i, n)    (((i) + (n) - 1) & ~((n) - 1))
static GstFlowReturn
gst_gapi_osd_transform_ip(GstBaseTransform *base, GstBuffer *buf)
{
    GstGapiosd *filter = GST_GAPI_OSD(base);
    GstVideoMeta * dmeta = NULL;

    if(filter->is_dma){
        dmeta = gst_buffer_get_video_meta(buf);
        if(dmeta && GST_VIDEO_INFO_FORMAT(&filter->src_info) == GST_VIDEO_FORMAT_NV12){
            GST_VIDEO_INFO_WIDTH(&filter->src_info) = ALIGN(GST_VIDEO_INFO_WIDTH(&filter->src_info), 64);
            GST_VIDEO_INFO_HEIGHT(&filter->src_info) = ALIGN(GST_VIDEO_INFO_HEIGHT(&filter->src_info), 64);
        }
    }

    g_rw_lock_reader_lock(&filter->rwlock);
    GList *list = filter->gapi_json_object_list;
    while (list != NULL) {
        GapiObject *object = (GapiObject *) list->data;
        GapiObjectClass *objectclass = G_API_OBJECT_TO_CLASS(object);
        objectclass->render_submit(object, filter->prims_pointer);
        list = list->next;
    }
    g_rw_lock_reader_unlock(&filter->rwlock);

    filter->gapi_buffer_object_list = parse_gststructure_from_roimeta(filter, buf);
    list = filter->gapi_buffer_object_list;
    while (list != NULL) {
        GapiObject *object = (GapiObject *) list->data;
        GapiObjectClass *objectclass = G_API_OBJECT_TO_CLASS(object);
        objectclass->render_submit(object, filter->prims_pointer);
        list = list->next;
    }
    render_sync(buf, &filter->sink_info, &filter->src_info, filter->prims_pointer);
    g_list_free_full(filter->gapi_buffer_object_list, (GDestroyNotify) g_object_unref);
    filter->gapi_buffer_object_list = NULL;

    return GST_FLOW_OK;
}

static void
gst_gapi_osd_finalize(GObject *object)
{
    g_assert(object != NULL);
    GstGapiosd *filter = GST_GAPI_OSD(object);
    if (filter->json_root) {
        json_object_put(filter->json_root);
    }
    g_free ((gpointer)filter->config_path);
    g_list_free_full(filter->gapi_json_object_list, (GDestroyNotify) g_object_unref);
    g_list_free_full(filter->gapi_configure_structure_list, (GDestroyNotify)gst_structure_free);
    if(filter->gapi_buffer_object_list != NULL) {
        g_list_free_full(filter->gapi_buffer_object_list, (GDestroyNotify) g_object_unref);
    }
    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    destory_array(filter->prims_pointer);
    g_rw_lock_clear(&filter->rwlock);
    G_OBJECT_CLASS(parent_class)->finalize(object);
}

GType
gst_gapiosd_backend_type_get_type(void)
{
    static volatile GType g_type;
    static const GEnumValue backend_type[] = {
        {BACK_END_TYPE_GAPI, "GAPI", "gapi" },
        {0, NULL, NULL }
    };
    if (g_once_init_enter(&g_type)) {
        GType type = g_enum_register_static("GstGapiosdBackEndType", backend_type);
        GST_INFO("Registering Gst gapiosd BackEnd Type");
        g_once_init_leave(&g_type, type);
    }
    return g_type;
}

static gboolean parse_from_json_file(GstGapiosd *filter)
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
            if (objectclass->parse_json(object, item) == false) {
                g_object_unref(object);
                continue;
            }
            filter->gapi_json_object_list = g_list_append(filter->gapi_json_object_list,
                                            object);
        }
    }
    return TRUE;
}

static GList* parse_gst_structure_list(GstGapiosd *filter, GList* structure_list)
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

static GList * parse_gststructure_from_roimeta(GstGapiosd *filter, GstBuffer *buffer)
{
    g_return_val_if_fail(buffer, NULL);

    GstMeta *gst_meta = NULL;
    gpointer state = NULL;
    GstVideoRegionOfInterestMeta *roi_meta = NULL;
    GList *index = NULL;
    GList *structure_list = NULL;
    GList *rect_list = NULL;
    GList *object_temp = NULL;
    GList *object_roi = NULL;
    gint label_id = 0;

    while ((gst_meta = gst_buffer_iterate_meta(buffer, &state)) != NULL) {
        if (gst_meta->info->api != GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE) {
            continue ;
        }
        roi_meta = (GstVideoRegionOfInterestMeta *)gst_meta;

        index = roi_meta->params;
        while(index) {
            GstStructure *gapiosd_s = (GstStructure *)index->data;
            if (gst_structure_has_field(gapiosd_s, "label_id") &&
                gst_structure_get_int(gapiosd_s, "label_id", &label_id)) {
                break;
            }
            index = g_list_next(index);
        }
        if(filter->drawroi) {
            GstStructure *s1 =
                gst_structure_new("gapiosd_meta",
                          "meta_id", G_TYPE_UINT, 1,
                          "meta_type", G_TYPE_STRING, "rect",
                          "x", G_TYPE_INT, roi_meta->x,
                          "y", G_TYPE_INT, roi_meta->y,
                          "width", G_TYPE_INT, roi_meta->w,
                          "height", G_TYPE_INT, roi_meta->h,
                          "r", G_TYPE_UINT, 0,
                          "g", G_TYPE_UINT, 0,
                          "b", G_TYPE_UINT, 0,
                          "thick", G_TYPE_INT, 5,
                          "lt", G_TYPE_INT, 8,
                          "shift", G_TYPE_INT, 0,
                          NULL);
            rect_list = g_list_append(rect_list, s1);
            char label[10] = {0};
            snprintf(label, 10, "%d", label_id);
            GstStructure *s2 =
               gst_structure_new("gapiosd_meta",
                         "meta_id", G_TYPE_UINT, 0,
                         "meta_type", G_TYPE_STRING, "text",
                         "text", G_TYPE_STRING, label,
                         "font_type", G_TYPE_INT, 0,
                         "font_scale", G_TYPE_DOUBLE, 2.0,
                         "x", G_TYPE_INT, roi_meta->x,
                         "y", G_TYPE_INT, roi_meta->y,
                         "r", G_TYPE_UINT, 0,
                         "g", G_TYPE_UINT, 0,
                         "b", G_TYPE_UINT, 0,
                         "line_thick", G_TYPE_INT, 1,
                         "line_type", G_TYPE_INT, 8,
                         "bottom_left_origin", G_TYPE_BOOLEAN, FALSE,
                         NULL);
           rect_list = g_list_append(rect_list, s2);
        }
        index = roi_meta->params;
        while(index) {
            GstStructure *gapiosd_s = (GstStructure *)index->data;
            std::string name = gst_structure_get_name(gapiosd_s);
            if(name == "gapiosd_meta") {
                GST_DEBUG_OBJECT(filter, "Structure type is gapiosd_meta\n");
                structure_list = g_list_append(structure_list, gapiosd_s);
            }

            index = g_list_next(index);
        }
    }
    if(structure_list != NULL) {
        object_temp = parse_gst_structure_list(filter, structure_list);
    }

    if(rect_list != NULL) {
        object_roi = parse_gst_structure_list(filter, rect_list);
        object_temp = g_list_concat(object_temp, object_roi);
        g_list_free_full(rect_list, (GDestroyNotify)gst_structure_free);
        rect_list = NULL;
    }
    if(object_temp == NULL) {
        GST_DEBUG_OBJECT(filter, "Can not get gobject for roimeta buffer !\n");
        return NULL;
    }
    return object_temp;
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

static const char *get_type_from_json(GstGapiosd *filter, json_object *item)
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

static const gchar *get_type_from_gst_struture(GstGapiosd *filter, GstStructure *struct_item)
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
find_info_by_type(GstGapiosd *filter, const char *item_type)
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
gst_gapi_osd_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps)
{
    g_assert(trans != NULL);
    g_assert(incaps != NULL);
    g_assert(outcaps != NULL);
    GstGapiosd *filter = GST_GAPI_OSD(trans);
    GstCapsFeatures *feature = NULL;
    if (!gst_video_info_from_caps(&filter->sink_info, incaps) ||
        !gst_video_info_from_caps(&filter->src_info, outcaps)) {
        GST_ERROR_OBJECT(filter, "invalid caps");
        return FALSE;
    }
    feature = gst_caps_get_features(outcaps, 0);
    if (gst_caps_features_contains(feature, "memory:DMABuf")) {
       filter->is_dma = TRUE;
    }
    return TRUE;
}

static gboolean
gst_gapi_osd_decide_allocation(GstBaseTransform *trans, GstQuery *query)
{
    GstGapiosd *filter = GST_GAPI_OSD(trans);
    GstCaps *caps = NULL;
    GstBufferPool *pool = NULL;
    guint size, min, max;
    GstVideoInfo info;
    gboolean update = FALSE;
    GstCapsFeatures *feature = NULL;
    GstAllocator *allocator = NULL;
    gst_query_parse_allocation(query, &caps, NULL);
    if (!caps) {
        goto error_no_caps;
    }

    GST_DEBUG_OBJECT(filter,
                     "decide gst_query_parse_allocation caps: %" GST_PTR_FORMAT, caps);

    feature = gst_caps_get_features(caps, 0);
    if (gst_caps_features_contains(feature, "memory:DMABuf")) {
        if (filter->allocator_name != NULL) {
            allocator =  gst_allocator_find(filter->allocator_name);
            if (!allocator) {
                goto error_no_dma_allocator;
            }
            if (gst_query_get_n_allocation_params(query) > 0) {
                gst_query_set_nth_allocation_param(query, 0, allocator, NULL);
            } else {
                gst_query_add_allocation_param(query, allocator, NULL);
            }
        } else {
            goto error_no_allocator_name;
        }
    }

    if (gst_query_get_n_allocation_pools(query) > 0) {
        gst_query_parse_nth_allocation_pool(query, 0, &pool, &size, &min, &max);
        update = TRUE;
    } else {
        pool = NULL;
        size = min = max = 0;
        pool = gst_buffer_pool_new();
    }

    if (!gst_video_info_from_caps(&info, caps)) {
        GST_DEBUG_OBJECT(trans, "invalid caps specified");
        return FALSE;
    }

    size = info.size;
    if (update) {
        gst_query_set_nth_allocation_pool(query, 0, pool, size, min, max);
    } else {
        gst_query_add_allocation_pool(query,  pool, size, min, max);
    }

    return  GST_BASE_TRANSFORM_CLASS(parent_class)->decide_allocation(trans, query);

error_no_caps:
    {
        GST_ERROR_OBJECT(trans, "no caps specified");
        return FALSE;
    }
error_no_allocator_name:
    {
        GST_ERROR_OBJECT(trans, "error_no_allocator_name");
        return FALSE;
    }
error_no_dma_allocator:
    {
        GST_ERROR_OBJECT(trans, "error_no_dma_allocator");
        return FALSE;
    }
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
    GST_DEBUG_CATEGORY_INIT(gst_gapi_osd_debug, "gapiosd", 0, "gapiosd plugin");
    return gst_element_register(plugin, "gapiosd", GST_RANK_NONE,
                                GST_TYPE_GAPI_OSD);
}

/* gstreamer looks for this structure to register plugins
 *
 * FIXME:exchange the string 'Template plugin' with you plugin description
 */
GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    gapiosd,
    "gapiosd plugin",
    plugin_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)

