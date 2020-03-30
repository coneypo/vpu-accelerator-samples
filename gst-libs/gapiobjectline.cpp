/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "gapiobjectline.h"
#define g_api_object_line_parent_class parent_class
G_DEFINE_TYPE(GApiObjectLine, g_api_object_line, G_TYPE_API_OJBECT);

static gboolean line_parse_json(GapiObject *apiobject,
                                json_object *json_object);
static gboolean line_parse_gst_structure(GapiObject *apiobject,
        GstStructure *structure);
static GstStructure *line_to_gst_structure(GapiObject *apiobject);
//it's used for do operations together
static void line_render_submit(GapiObject *apiobject, gpointer prims_pointer);
//it's used for do operations one by one
static gboolean line_render_ip(GapiObject *apiobject, GstVideoInfo *sink_info,
                               GstVideoInfo *src_info, GstBuffer *buf);
//it's used for video scale,video crop, video format convert, or just don't want to modify the origin buffer
static gboolean line_render(GapiObject *apiobject, GstBuffer *in_buf,
                            GstBuffer *out_buf);

/* initialize the plugin's class */
static void
g_api_object_line_class_init(GApiObjectLineClass *klass)
{
    GapiObjectClass *gapi_object_class;
    gapi_object_class = (GapiObjectClass *) klass;
    gapi_object_class->parse_json = line_parse_json;
    gapi_object_class->parse_gst_structure = line_parse_gst_structure;
    gapi_object_class->to_gst_structure = line_to_gst_structure;
    gapi_object_class->render_submit = line_render_submit;
    gapi_object_class->render_ip = line_render_ip;
    gapi_object_class->render = line_render;
}

/* initialize the new element
 * initialize instance structure
 */

static void
g_api_object_line_init(GApiObjectLine *object)
{
    object->lineInfo.pt1.x = 100;
    object->lineInfo.pt1.y = 100;
    object->lineInfo.pt2.x = 300;
    object->lineInfo.pt2.y = 300;
    object->lineInfo.color = cv::Scalar(0, 0, 0);
    object->lineInfo.thick = 1;
    object->lineInfo.lt = 8;
    object->lineInfo.shift = 0;
}
/*json_design:*/
/*{*/
/*"meta_id":2,*/
/*"meta_type":"line",*/
/*"x":100,*/
/*"y":100,*/
/*"x":300,*/
/*"y":300,*/
/*"color_rgb":[0,0,0],*/
/*"thick":1,*/
/*"lt":8,*/
/*"shift":0*/
/*}*/
static gboolean line_parse_json(GapiObject *apiobject,
                                json_object *json_object)
{
    g_assert(apiobject != NULL);
    g_assert(json_object != NULL);
    GApiObjectLine *object = G_API_OBJECT_LINE(apiobject);
    const char *meta_type_str = gapiosd_json_get_string(json_object, "meta_type");
    if (meta_type_str == NULL) {
        return FALSE;
    }
    if (std::string(meta_type_str) != std::string("line")) {
        GST_ERROR_OBJECT(object, "json_object with wrong meta_type \n");
        return FALSE;
    }
    gapiosd_json_get_int(json_object, "x", &(object->lineInfo.pt1.x));
    gapiosd_json_get_int(json_object, "y", &(object->lineInfo.pt1.y));
    gapiosd_json_get_int(json_object, "x2", &(object->lineInfo.pt2.x));
    gapiosd_json_get_int(json_object, "y2", &(object->lineInfo.pt2.y));
    gapiosd_json_get_rgb(json_object, "color_rgb", &(object->lineInfo.color));
    gapiosd_json_get_int(json_object, "thick", &(object->lineInfo.thick));
    gapiosd_json_get_int(json_object, "lt", &(object->lineInfo.lt));
    gapiosd_json_get_int(json_object, "shift", &(object->lineInfo.shift));
    return G_API_OBJECT_CLASS(parent_class)->parse_json(apiobject, json_object);
}

/*structure_design:*/
/*typedef struct Line*/
/*{*/
/*uint "meta_id"*/
/*string "meta_type"*/
/*int "x"*/
/*int "y"*/
/*int "x2"*/
/*int "y2"*/
/*uint "r"*/
/*uint "g"*/
/*uint "b"*/
/*int "thick"*/
/*int "lt"*/
/*int "shift";*/
/*}Line;*/
static gboolean line_parse_gst_structure(GapiObject *apiobject,
        GstStructure *structure)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && structure != NULL), FALSE);
    GApiObjectLine *object = G_API_OBJECT_LINE(apiobject);
    if (!gst_structure_has_name(structure, "gapiosd_meta")) {
        GST_ERROR_OBJECT(object, "GstStructure with a wrong name !\n");
        return FALSE;
    }
    guint R = -1, G = -1, B = -1;
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "x",
                       &(object->lineInfo.pt1.x)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "y",
                       &(object->lineInfo.pt1.y)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "x2",
                       &(object->lineInfo.pt2.x)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "y2",
                       &(object->lineInfo.pt2.y)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_uint(structure, "r", &R), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_uint(structure, "g", &G), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_uint(structure, "b", &B), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "thick",
                       &(object->lineInfo.thick)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "lt",
                       &(object->lineInfo.lt)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "shift",
                       &(object->lineInfo.shift)), FALSE);
    object->lineInfo.color = cv::Scalar(R, G, B);
    return G_API_OBJECT_CLASS(parent_class)->parse_gst_structure(apiobject, structure);
}
/*structure_design:*/
/*typedef struct Line*/
/*{*/
/*uint "meta_id"*/
/*string "meta_type"*/
/*int "x"*/
/*int "y"*/
/*int "x2"*/
/*int "y2"*/
/*uint "r"*/
/*uint "g"*/
/*uint "b"*/
/*int "thick"*/
/*int "lt"*/
/*int "shift";*/
/*}Line;*/
static GstStructure *line_to_gst_structure(GapiObject *apiobject)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL), NULL);
    GApiObjectLine *object = G_API_OBJECT_LINE(apiobject);
    GstStructure *s =
        gst_structure_new("gapiosd_meta",
                          "meta_id", G_TYPE_UINT, apiobject->meta_id,
                          "meta_type", G_TYPE_STRING, apiobject->meta_type,
                          "x", G_TYPE_INT, object->lineInfo.pt1.x,
                          "y", G_TYPE_INT, object->lineInfo.pt1.y,
                          "x2", G_TYPE_INT, object->lineInfo.pt2.x,
                          "y2", G_TYPE_INT, object->lineInfo.pt2.y,
                          "r", G_TYPE_UINT, (guint)object->lineInfo.color[0],
                          "g", G_TYPE_UINT, (guint)object->lineInfo.color[1],
                          "b", G_TYPE_UINT, (guint)object->lineInfo.color[2],
                          "thick", G_TYPE_INT, object->lineInfo.thick,
                          "lt", G_TYPE_INT, object->lineInfo.lt,
                          "shift", G_TYPE_INT, object->lineInfo.shift,
                          NULL);
    return s;
}
//it's used for do operations together
static void line_render_submit(GapiObject *apiobject, gpointer prims_pointer)
{
    g_assert(apiobject != NULL);
    g_assert(prims_pointer != NULL);
    GApiObjectLine *object = G_API_OBJECT_LINE(apiobject);
    ((cv::gapi::wip::draw::Prims *)prims_pointer)->emplace_back(cv::gapi::wip::draw::Line(object->lineInfo));
    return;
}
//it's used for do operations one by one
static gboolean line_render_ip(GapiObject *apiobject, GstVideoInfo *sink_info,
                               GstVideoInfo *src_info, GstBuffer *buf)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && buf != NULL), FALSE);
    GApiObjectLine *object = G_API_OBJECT_LINE(apiobject);
    UNUSED(object);
    return TRUE;
}
//it's used for video scale,video crop, video format convert, or just don't want to modify the origin buffer
static gboolean line_render(GapiObject *apiobject, GstBuffer *in_buf,
                            GstBuffer *out_buf)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && in_buf != NULL
                        && out_buf != NULL), FALSE);
    GApiObjectLine *object = G_API_OBJECT_LINE(apiobject);
    UNUSED(object);
    return TRUE;
}

GapiObject *gapiobjectLine_create()
{
    return (GapiObject *)g_object_new(g_api_object_line_get_type(), NULL);
}
