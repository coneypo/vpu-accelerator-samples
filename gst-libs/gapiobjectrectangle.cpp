/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "gapiobjectrectangle.h"
#define g_api_object_rect_parent_class parent_class
G_DEFINE_TYPE(GApiObjectRect, g_api_object_rect, G_TYPE_API_OJBECT);

static gboolean rect_parse_json(GapiObject *apiobject,
                                json_object *json_object);
static gboolean rect_parse_gst_structure(GapiObject *apiobject,
        GstStructure *structure);
static GstStructure *rect_to_gst_structure(GapiObject *apiobject);
//it's used for do operations together
static void rect_render_submit(GapiObject *apiobject, gpointer prims_pointer);
//it's used for do operations one by one
static gboolean rect_render_ip(GapiObject *apiobject, GstVideoInfo *sink_info,
                               GstVideoInfo *src_info, GstBuffer *buf);
//it's used for video scale,video crop, video format convert, or just don't want to modify the origin buffer
static gboolean rect_render(GapiObject *apiobject, GstBuffer *in_buf,
                            GstBuffer *out_buf);

/* initialize the plugin's class */
static void
g_api_object_rect_class_init(GApiObjectRectClass *klass)
{
    GapiObjectClass *gapi_object_class;
    gapi_object_class = (GapiObjectClass *) klass;
    gapi_object_class->parse_json = rect_parse_json;
    gapi_object_class->parse_gst_structure = rect_parse_gst_structure;
    gapi_object_class->to_gst_structure = rect_to_gst_structure;
    gapi_object_class->render_submit = rect_render_submit;
    gapi_object_class->render_ip = rect_render_ip;
    gapi_object_class->render = rect_render;
}

/* initialize the new element
 * initialize instance structure
 */

static void
g_api_object_rect_init(GApiObjectRect *object)
{
    object->rectInfo.rect.x = 100;
    object->rectInfo.rect.y = 100;
    object->rectInfo.rect.width = 100;
    object->rectInfo.rect.height = 100;
    object->rectInfo.color = cv::Scalar(0, 0, 0);
    object->rectInfo.thick = 1;
    object->rectInfo.lt = 8;
    object->rectInfo.shift = 0;
}
/*json_design:*/
/*{*/
/*"meta_id":2,*/
/*"meta_type":"rect",*/
/*"x":100,*/
/*"y":100,*/
/*"width":25,*/
/*"height":20,*/
/*"color_rgb":[0,0,0],*/
/*"thick":1,*/
/*"lt":8,*/
/*"shift":5*/
/*}*/
static gboolean rect_parse_json(GapiObject *apiobject,
                                json_object *json_object)
{
    g_assert(apiobject != NULL);
    g_assert(json_object != NULL);
    GApiObjectRect *object = G_API_OBJECT_RECT(apiobject);
    const char *meta_type_str = gapiosd_json_get_string(json_object, "meta_type");
    if (meta_type_str == NULL) {
        return FALSE;
    }
    if (std::string(meta_type_str) != std::string("rect")) {
        GST_ERROR_OBJECT(object, "json_object with wrong meta_type \n");
        return FALSE;
    }
    gapiosd_json_get_int(json_object, "x", &(object->rectInfo.rect.x));
    gapiosd_json_get_int(json_object, "y", &(object->rectInfo.rect.y));
    gapiosd_json_get_int(json_object, "width", &(object->rectInfo.rect.width));
    gapiosd_json_get_int(json_object, "height", &(object->rectInfo.rect.height));
    gapiosd_json_get_rgb(json_object, "color_rgb", &(object->rectInfo.color));
    gapiosd_json_get_int(json_object, "thick", &(object->rectInfo.thick));
    gapiosd_json_get_int(json_object, "lt", &(object->rectInfo.lt));
    gapiosd_json_get_int(json_object, "shift", &(object->rectInfo.shift));
    return G_API_OBJECT_CLASS(parent_class)->parse_json(apiobject, json_object);
}

/*structure_design:*/
/*typedef struct Rect*/
/*{*/
/*uint "meta_id"*/
/*string "meta_type"*/
/*int "x"*/
/*int "y"*/
/*int "width"*/
/*int "height"*/
/*uint "r"*/
/*uint "g"*/
/*uint "b"*/
/*int "thick"*/
/*int "lt"*/
/*int "shift";*/
/*}Rect;*/
static gboolean rect_parse_gst_structure(GapiObject *apiobject,
        GstStructure *structure)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && structure != NULL), FALSE);
    GApiObjectRect *object = G_API_OBJECT_RECT(apiobject);
    if (!gst_structure_has_name(structure, "gapiosd_meta")) {
        GST_ERROR_OBJECT(object, "GstStructure with a wrong name !\n");
        return FALSE;
    }
    guint R = -1, G = -1, B = -1;
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "x",
                       &(object->rectInfo.rect.x)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "y",
                       &(object->rectInfo.rect.y)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "width",
                       &(object->rectInfo.rect.width)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "height",
                       &(object->rectInfo.rect.height)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_uint(structure, "r", &R), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_uint(structure, "g", &G), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_uint(structure, "b", &B), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "thick",
                       &(object->rectInfo.thick)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "lt",
                       &(object->rectInfo.lt)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "shift",
                       &(object->rectInfo.shift)), FALSE);
    object->rectInfo.color = cv::Scalar(R, G, B);
    return G_API_OBJECT_CLASS(parent_class)->parse_gst_structure(apiobject, structure);
}
/*structure_design:*/
/*typedef struct Rect*/
/*{*/
/*uint "meta_id"*/
/*string "meta_type"*/
/*int "x"*/
/*int "y"*/
/*int "width"*/
/*int "height"*/
/*uint "r"*/
/*uint "g"*/
/*uint "b"*/
/*int "thick"*/
/*int "lt"*/
/*int "shift";*/
/*}Rect;*/
static GstStructure *rect_to_gst_structure(GapiObject *apiobject)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL), NULL);
    GApiObjectRect *object = G_API_OBJECT_RECT(apiobject);
    GstStructure *s =
        gst_structure_new("gapiosd_meta",
                          "meta_id", G_TYPE_UINT, apiobject->meta_id,
                          "meta_type", G_TYPE_STRING, apiobject->meta_type,
                          "x", G_TYPE_INT, object->rectInfo.rect.x,
                          "y", G_TYPE_INT, object->rectInfo.rect.y,
                          "width", G_TYPE_INT, object->rectInfo.rect.width,
                          "height", G_TYPE_INT, object->rectInfo.rect.height,
                          "r", G_TYPE_UINT, (guint)object->rectInfo.color[0],
                          "g", G_TYPE_UINT, (guint)object->rectInfo.color[1],
                          "b", G_TYPE_UINT, (guint)object->rectInfo.color[2],
                          "thick", G_TYPE_INT, object->rectInfo.thick,
                          "lt", G_TYPE_INT, object->rectInfo.lt,
                          "shift", G_TYPE_INT, object->rectInfo.shift,
                          NULL);
    return s;
}
//it's used for do operations together
static void rect_render_submit(GapiObject *apiobject, gpointer prims_pointer)
{
    g_assert(apiobject != NULL);
    g_assert(prims_pointer != NULL);
    GApiObjectRect *object = G_API_OBJECT_RECT(apiobject);
    ((cv::gapi::wip::draw::Prims *)prims_pointer)->emplace_back(cv::gapi::wip::draw::Rect(object->rectInfo));
    return;
}
//it's used for do operations one by one
static gboolean rect_render_ip(GapiObject *apiobject, GstVideoInfo *sink_info,
                               GstVideoInfo *src_info, GstBuffer *buf)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && buf != NULL), FALSE);
    GApiObjectRect *object = G_API_OBJECT_RECT(apiobject);
    UNUSED(object);
    return TRUE;
}
//it's used for video scale,video crop, video format convert, or just don't want to modify the origin buffer
static gboolean rect_render(GapiObject *apiobject, GstBuffer *in_buf,
                            GstBuffer *out_buf)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && in_buf != NULL
                        && out_buf != NULL), FALSE);
    GApiObjectRect *object = G_API_OBJECT_RECT(apiobject);
    UNUSED(object);
    return TRUE;
}

GapiObject *gapiobjectRect_create()
{
    return (GapiObject *)g_object_new(g_api_object_rect_get_type(), NULL);
}
