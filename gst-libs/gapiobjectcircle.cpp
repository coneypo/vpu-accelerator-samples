/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "gapiobjectcircle.h"
#define g_api_object_circle_parent_class parent_class
G_DEFINE_TYPE(GApiObjectCircle, g_api_object_circle, G_TYPE_API_OJBECT);

static gboolean circle_parse_json(GapiObject *apiobject,
                                json_object *json_object);
static gboolean circle_parse_gst_structure(GapiObject *apiobject,
        GstStructure *structure);
static GstStructure *circle_to_gst_structure(GapiObject *apiobject);
//it's used for do operations together
static void circle_render_submit(GapiObject *apiobject, gpointer prims_pointer);
//it's used for do operations one by one
static gboolean circle_render_ip(GapiObject *apiobject, GstVideoInfo *sink_info,
                               GstVideoInfo *src_info, GstBuffer *buf);
static gboolean circle_render(GapiObject *apiobject, GstBuffer *in_buf,
                            GstBuffer *out_buf);

/* initialize the plugin's class */
static void
g_api_object_circle_class_init(GApiObjectCircleClass *klass)
{
    GapiObjectClass *gapi_object_class;
    gapi_object_class = (GapiObjectClass *) klass;
    gapi_object_class->parse_json = circle_parse_json;
    gapi_object_class->parse_gst_structure = circle_parse_gst_structure;
    gapi_object_class->to_gst_structure = circle_to_gst_structure;
    gapi_object_class->render_submit = circle_render_submit;
    gapi_object_class->render_ip = circle_render_ip;
    gapi_object_class->render = circle_render;
}

/* initialize the new element
 * initialize instance structure
 */
static void
g_api_object_circle_init(GApiObjectCircle *object)
{
    object->circleInfo.center.x = 100;
    object->circleInfo.center.y = 200;
    object->circleInfo.radius = 50;
    object->circleInfo.color = cv::Scalar(0, 0, 0);
    object->circleInfo.thick = 2;
    object->circleInfo.lt = 8;
    object->circleInfo.shift = 0;
}
/*json_design:*/
/*{*/
/*"meta_id":4,*/
/*"meta_type":"circle",*/
/*"x":100,*/
/*"y":200,*/
/*"radius":50,*/
/*"color_rgb":[0,0,0],*/
/*"thick":2,*/
/*"lt":8,*/
/*"shift":0*/
/*}*/
static gboolean circle_parse_json(GapiObject *apiobject,
                                json_object *json_object)
{
    g_assert(apiobject != NULL);
    g_assert(json_object != NULL);
    GApiObjectCircle *object = G_API_OBJECT_CIRCLE(apiobject);
    if (std::string(gapiosd_json_get_string(json_object, "meta_type")) != std::string("circle")) {
        GST_ERROR_OBJECT(object, "json_object with wrong meta_type \n");
        return FALSE;
    }
    gapiosd_json_get_int(json_object, "x", &(object->circleInfo.center.x));
    gapiosd_json_get_int(json_object, "y", &(object->circleInfo.center.y));
    gapiosd_json_get_int(json_object, "radius", &(object->circleInfo.radius));
    gapiosd_json_get_rgb(json_object, "color_rgb", &(object->circleInfo.color));
    gapiosd_json_get_int(json_object, "thick", &(object->circleInfo.thick));
    gapiosd_json_get_int(json_object, "lt", &(object->circleInfo.lt));
    gapiosd_json_get_int(json_object, "shift", &(object->circleInfo.shift));
    return TRUE;
}

/*structure_design:*/
/*typedef struct Circle*/
/*{*/
/*uint "meta_id"*/
/*string "meta_type"*/
/*int "x"*/
/*int "y"*/
/*int "radius"*/
/*uint "r"*/
/*uint "g"*/
/*uint "b"*/
/*int "thick"*/
/*int "lt"*/
/*int "shift";*/
/*}Circle;*/
static gboolean circle_parse_gst_structure(GapiObject *apiobject,
        GstStructure *structure)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && structure != NULL), FALSE);
    GApiObjectCircle *object = G_API_OBJECT_CIRCLE(apiobject);
    if (!gst_structure_has_name(structure, "gapiosd_meta")) {
        GST_ERROR_OBJECT(object, "GstStructure with a wrong name !\n");
        return FALSE;
    }
    guint R = -1, G = -1, B = -1;
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "x",
                       &(object->circleInfo.center.x)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "y",
                       &(object->circleInfo.center.y)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "radius",
                       &(object->circleInfo.radius)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_uint(structure, "r", &R), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_uint(structure, "g", &G), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_uint(structure, "b", &B), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "thick",
                       &(object->circleInfo.thick)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "lt",
                       &(object->circleInfo.lt)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "shift",
                       &(object->circleInfo.shift)), FALSE);
    object->circleInfo.color = cv::Scalar(R, G, B);
    return TRUE;
}

static GstStructure *circle_to_gst_structure(GapiObject *apiobject)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL), NULL);
    GApiObjectCircle *object = G_API_OBJECT_CIRCLE(apiobject);
    GstStructure *s =
        gst_structure_new("gapiosd_meta",
                          "meta_id", G_TYPE_UINT, 1,
                          "meta_type", G_TYPE_STRING, "circle",
                          "x", G_TYPE_INT, object->circleInfo.center.x,
                          "y", G_TYPE_INT, object->circleInfo.center.y,
                          "radius", G_TYPE_INT, object->circleInfo.radius,
                          "r", G_TYPE_UINT, (guint)object->circleInfo.color[0],
                          "g", G_TYPE_UINT, (guint)object->circleInfo.color[1],
                          "b", G_TYPE_UINT, (guint)object->circleInfo.color[2],
                          "thick", G_TYPE_INT, object->circleInfo.thick,
                          "lt", G_TYPE_INT, object->circleInfo.lt,
                          "shift", G_TYPE_INT, object->circleInfo.shift,
                          NULL);
    return s;
}
//it's used for do operations together
static void circle_render_submit(GapiObject *apiobject, gpointer prims_pointer)
{
    g_assert(apiobject != NULL);
    g_assert(prims_pointer != NULL);
    GApiObjectCircle *object = G_API_OBJECT_CIRCLE(apiobject);
    ((cv::gapi::wip::draw::Prims *)prims_pointer)->emplace_back(cv::gapi::wip::draw::Circle(object->circleInfo));
    return;
}
//it's used for do operations one by one
static gboolean circle_render_ip(GapiObject *apiobject, GstVideoInfo *sink_info,
                               GstVideoInfo *src_info, GstBuffer *buf)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && buf != NULL), FALSE);
    GApiObjectCircle *object = G_API_OBJECT_CIRCLE(apiobject);
    return TRUE;
}
static gboolean circle_render(GapiObject *apiobject, GstBuffer *in_buf,
                            GstBuffer *out_buf)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && in_buf != NULL
                        && out_buf != NULL), FALSE);
    GApiObjectCircle *object = G_API_OBJECT_CIRCLE(apiobject);
    return TRUE;
}

GapiObject *gapiobjectCircle_create()
{
    return (GapiObject *)g_object_new(g_api_object_circle_get_type(), NULL);
}
