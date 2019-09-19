/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "gapiobjecttext.h"
#define g_api_object_text_parent_class parent_class
G_DEFINE_TYPE(GApiObjectText, g_api_object_text, G_TYPE_API_OJBECT);

static gboolean text_parse_json(GapiObject *apiobject,
                                json_object *json_object);
static gboolean text_parse_gst_structure(GapiObject *apiobject,
        GstStructure *structure);
static GstStructure *text_to_gst_structure(GapiObject *apiobject);
//it's used for do operations together
static void text_render_submit(GapiObject *apiobject, gpointer prims_pointer);
//it's used for do operations one by one
static gboolean text_render_ip(GapiObject *apiobject, GstVideoInfo *sink_info,
                               GstVideoInfo *src_info, GstBuffer *buf);
//it's used for video scale,video crop, video format convert, or just don't want to modify the origin buffer
static gboolean text_render(GapiObject *apiobject, GstBuffer *in_buf,
                            GstBuffer *out_buf);

/* initialize the plugin's class */
static void
g_api_object_text_class_init(GApiObjectTextClass *klass)
{
    GapiObjectClass *gapi_object_class;
    gapi_object_class = (GapiObjectClass *) klass;
    gapi_object_class->parse_json = text_parse_json;
    gapi_object_class->parse_gst_structure = text_parse_gst_structure;
    gapi_object_class->to_gst_structure = text_to_gst_structure;
    gapi_object_class->render_submit = text_render_submit;
    gapi_object_class->render_ip = text_render_ip;
    gapi_object_class->render = text_render;
}

/* initialize the new element
 * initialize instance structure
 */
static void
g_api_object_text_init(GApiObjectText *object)
{
    object->textInfo.text = "testInit";
    object->textInfo.org.x = 100;
    object->textInfo.org.y = 100;
    object->textInfo.ff = 0;
    object->textInfo.fs = 2.0;
    object->textInfo.color = cv::Scalar(0, 0, 0);
    object->textInfo.thick = 1;
    object->textInfo.lt = 8;
    object->textInfo.bottom_left_origin = FALSE;
}
/*json_design:*/
/*{*/
/*"meta_id":1,*/
/*"meta_type":"text",*/
/*"text":"test123",*/
/*"font_type":0,*/
/*"font_scale":0.1,*/
/*"x":0,*/
/*"y":0,*/
/*"color_rgb":[0,255,2]*/
/*"line_thick":3,*/
/*"line_type":1,*/
/*"bottom_left_origin":0*/
/*};*/
static gboolean text_parse_json(GapiObject *apiobject,
                                json_object *json_object)
{
    g_assert(apiobject != NULL);
    g_assert(json_object != NULL);
    GApiObjectText *object = G_API_OBJECT_TEXT(apiobject);
    if (std::string(gapiosd_json_get_string(json_object, "meta_type")) != std::string("text")) {
        GST_ERROR_OBJECT(object, "json_object with wrong meta_type \n");
        return FALSE;
    }
    object->textInfo.text = gapiosd_json_get_string(json_object, "text");
    gapiosd_json_get_int(json_object, "font_type", &(object->textInfo.ff));
    gapiosd_json_get_double(json_object, "font_scale", &(object->textInfo.fs));
    gapiosd_json_get_int(json_object, "x", &(object->textInfo.org.x));
    gapiosd_json_get_int(json_object, "y", &(object->textInfo.org.y));
    gapiosd_json_get_rgb(json_object, "color_rgb", &(object->textInfo.color));
    gapiosd_json_get_int(json_object, "line_thick", &(object->textInfo.thick));
    gapiosd_json_get_int(json_object, "line_type", &(object->textInfo.lt));
    object->textInfo.bottom_left_origin = gapiosd_json_check_enable_state(json_object,
                                          "bottom_left_origin");
    return TRUE;
}

/*structure_design:*/
/*typedef struct Text*/
/*{*/
/*uint "meta_id"*/
/*string "meta_type"*/
/*string "text";*/
/*int "font_type"*/
/*double "font_scale"*/
/*int "x"*/
/*int "y"*/
/*uint "r"*/
/*uint "g"*/
/*uint "b"*/
/*int "line_thick"*/
/*bool "bottom_left_origin";*/
/*}Text;*/
static gboolean text_parse_gst_structure(GapiObject *apiobject,
        GstStructure *structure)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && structure != NULL), FALSE);
    GApiObjectText *object = G_API_OBJECT_TEXT(apiobject);
    if (!gst_structure_has_name(structure, "gapiosd_meta")) {
        GST_ERROR_OBJECT(object, "GstStructure with a wrong name !\n");
        return FALSE;
    }
    guint R = -1, G = -1, B = -1;
    object->textInfo.text = gst_structure_get_string(structure, "text");
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "font_type",
                       &(object->textInfo.ff)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_double(structure, "font_scale",
                       &(object->textInfo.fs)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "x",
                       &(object->textInfo.org.x)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "y",
                       &(object->textInfo.org.y)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_uint(structure, "r", &R), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_uint(structure, "g", &G), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_uint(structure, "b", &B), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "line_thick",
                       &(object->textInfo.lt)), FALSE);
    gboolean boolBuf;
    RETURN_VAL_IF_FAIL(gst_structure_get_boolean(structure, "bottom_left_origin",
                       &boolBuf), FALSE);
    object->textInfo.bottom_left_origin = boolBuf;
    object->textInfo.color = cv::Scalar(R, G, B);

    return TRUE;
}
/*structure_design:*/
/*typedef struct Text*/
/*{*/
/*uint "meta_id"*/
/*string "meta_type"*/
/*string "text";*/
/*int "font_type"*/
/*double "font_scale"*/
/*int "x"*/
/*int "y"*/
/*uint "r"*/
/*uint "g"*/
/*uint "b"*/
/*int "line_thick"*/
/*bool "bottom_left_origin";*/
/*}Text;*/
static GstStructure *text_to_gst_structure(GapiObject *apiobject)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL), NULL);
    GApiObjectText *object = G_API_OBJECT_TEXT(apiobject);
    GstStructure *s =
        gst_structure_new("gapiosd_meta",
                          "meta_id", G_TYPE_UINT, 0,
                          "meta_type", G_TYPE_STRING, "text",
                          "text", G_TYPE_STRING, object->textInfo.text.c_str(),
                          "font_type", G_TYPE_INT, object->textInfo.ff,
                          "font_scale", G_TYPE_DOUBLE, object->textInfo.fs,
                          "x", G_TYPE_INT, object->textInfo.org.x,
                          "y", G_TYPE_INT, object->textInfo.org.y,
                          "r", G_TYPE_UINT, (guint)object->textInfo.color[0],
                          "g", G_TYPE_UINT, (guint)object->textInfo.color[1],
                          "b", G_TYPE_UINT, (guint)object->textInfo.color[2],
                          "line_thick", G_TYPE_INT, object->textInfo.lt,
                          "bottom_left_origin", G_TYPE_BOOLEAN, object->textInfo.bottom_left_origin,
                          NULL);
    return s;
}
//it's used for do operations together
static void text_render_submit(GapiObject *apiobject, gpointer prims_pointer)
{
    g_assert(apiobject != NULL);
    g_assert(prims_pointer != NULL);
    GApiObjectText *object = G_API_OBJECT_TEXT(apiobject);
    //AllPrims.emplace_back(object->textInfo);
    //AllPrims.emplace_back(cv::gapi::wip::draw::Text{"testInit",cv::Point(100,100), 0, 2.0,cv::Scalar(0, 0, 0),1,8,0 });
    ((cv::gapi::wip::draw::Prims *)prims_pointer)->emplace_back(cv::gapi::wip::draw::Text(object->textInfo));
    return;
}
//it's used for do operations one by one
static gboolean text_render_ip(GapiObject *apiobject, GstVideoInfo *sink_info,
                               GstVideoInfo *src_info, GstBuffer *buf)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && buf != NULL), FALSE);
    GApiObjectText *object = G_API_OBJECT_TEXT(apiobject);
    return TRUE;
}
//it's used for video scale,video crop, video format convert, or just don't want to modify the origin buffer
static gboolean text_render(GapiObject *apiobject, GstBuffer *in_buf,
                            GstBuffer *out_buf)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && in_buf != NULL
                        && out_buf != NULL), FALSE);
    GApiObjectText *object = G_API_OBJECT_TEXT(apiobject);
    return TRUE;
}

GapiObject *gapiobjectText_create()
{
    return (GapiObject *)g_object_new(g_api_object_text_get_type(), NULL);
}
