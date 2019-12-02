/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "gapiobjectmosaic.h"
#define g_api_object_mosaic_parent_class parent_class
G_DEFINE_TYPE(GApiObjectMosaic, g_api_object_mosaic, G_TYPE_API_OJBECT);

static gboolean mosaic_parse_json(GapiObject *apiobject,
                                json_object *json_object);
static gboolean mosaic_parse_gst_structure(GapiObject *apiobject,
        GstStructure *structure);
static GstStructure *mosaic_to_gst_structure(GapiObject *apiobject);
//it's used for do operations together
static void mosaic_render_submit(GapiObject *apiobject, gpointer prims_pointer);
//it's used for do operations one by one
static gboolean mosaic_render_ip(GapiObject *apiobject, GstVideoInfo *sink_info,
                               GstVideoInfo *src_info, GstBuffer *buf);
//it's used for video scale,video crop, video format convert, or just don't want to modify the origin buffer
static gboolean mosaic_render(GapiObject *apiobject, GstBuffer *in_buf,
                            GstBuffer *out_buf);

/* initialize the plugin's class */
static void
g_api_object_mosaic_class_init(GApiObjectMosaicClass *klass)
{
    GapiObjectClass *gapi_object_class;
    gapi_object_class = (GapiObjectClass *) klass;
    gapi_object_class->parse_json = mosaic_parse_json;
    gapi_object_class->parse_gst_structure = mosaic_parse_gst_structure;
    gapi_object_class->to_gst_structure = mosaic_to_gst_structure;
    gapi_object_class->render_submit = mosaic_render_submit;
    gapi_object_class->render_ip = mosaic_render_ip;
    gapi_object_class->render = mosaic_render;
}

/* initialize the new element
 * initialize instance structure
 */

static void
g_api_object_mosaic_init(GApiObjectMosaic *object)
{
    object->mosaicInfo.mos.x = 100;
    object->mosaicInfo.mos.y = 100;
    object->mosaicInfo.mos.width = 100;
    object->mosaicInfo.mos.height = 100;
    object->mosaicInfo.cellSz = 5;
    object->mosaicInfo.decim = 0;
}
/*json_design:*/
/*{*/
/*"meta_id":2,*/
/*"meta_type":"mosaic",*/
/*"x":100,*/
/*"y":100,*/
/*"width":100,*/
/*"height":100,*/
/*"cellSz":5,*/
/*"decim":0*/
/*}*/
static gboolean mosaic_parse_json(GapiObject *apiobject,
                                json_object *json_object)
{
    g_assert(apiobject != NULL);
    g_assert(json_object != NULL);
    GApiObjectMosaic *object = G_API_OBJECT_MOSAIC(apiobject);
    const char *meta_type_str = gapiosd_json_get_string(json_object, "meta_type");
    if (meta_type_str == NULL) {
        return FALSE;
    }
    if (std::string(meta_type_str) != std::string("mosaic")) {
        GST_ERROR_OBJECT(object, "json_object with wrong meta_type \n");
        return FALSE;
    }
    gapiosd_json_get_int(json_object, "x", &(object->mosaicInfo.mos.x));
    gapiosd_json_get_int(json_object, "y", &(object->mosaicInfo.mos.y));
    gapiosd_json_get_int(json_object, "width", &(object->mosaicInfo.mos.width));
    gapiosd_json_get_int(json_object, "height", &(object->mosaicInfo.mos.height));
    gapiosd_json_get_int(json_object, "cellSz", &(object->mosaicInfo.cellSz));
    gapiosd_json_get_int(json_object, "decim", &(object->mosaicInfo.decim));
    return G_API_OBJECT_CLASS(parent_class)->parse_json(apiobject, json_object);
}

/*structure_design:*/
/*typedef struct Mosaic*/
/*{*/
/*uint "meta_id"*/
/*string "meta_type"*/
/*int "x"*/
/*int "y"*/
/*int "width"*/
/*int "height"*/
/*int "cellSz"*/
/*int "decim";*/
/*}Mosaic;*/
static gboolean mosaic_parse_gst_structure(GapiObject *apiobject,
        GstStructure *structure)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && structure != NULL), FALSE);
    GApiObjectMosaic *object = G_API_OBJECT_MOSAIC(apiobject);
    if (!gst_structure_has_name(structure, "gapiosd_meta")) {
        GST_ERROR_OBJECT(object, "GstStructure with a wrong name !\n");
        return FALSE;
    }
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "x",
                       &(object->mosaicInfo.mos.x)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "y",
                       &(object->mosaicInfo.mos.y)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "width",
                       &(object->mosaicInfo.mos.width)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "height",
                       &(object->mosaicInfo.mos.height)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "cellSz",
                       &(object->mosaicInfo.cellSz)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "decim",
                       &(object->mosaicInfo.decim)), FALSE);
    return G_API_OBJECT_CLASS(parent_class)->parse_gst_structure(apiobject, structure);
}
/*structure_design:*/
/*typedef struct Mosaic*/
/*{*/
/*uint "meta_id"*/
/*string "meta_type"*/
/*int "x"*/
/*int "y"*/
/*int "width"*/
/*int "height"*/
/*int "cellSz"*/
/*int "decim";*/
/*}Mosaic;*/
static GstStructure *mosaic_to_gst_structure(GapiObject *apiobject)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL), NULL);
    GApiObjectMosaic *object = G_API_OBJECT_MOSAIC(apiobject);
    GstStructure *s =
        gst_structure_new("gapiosd_meta",
                          "meta_id", G_TYPE_UINT, apiobject->meta_id,
                          "meta_type", G_TYPE_STRING, apiobject->meta_type,
                          "x", G_TYPE_INT, object->mosaicInfo.mos.x,
                          "y", G_TYPE_INT, object->mosaicInfo.mos.y,
                          "width", G_TYPE_INT, object->mosaicInfo.mos.width,
                          "height", G_TYPE_INT, object->mosaicInfo.mos.height,
                          "cellSz", G_TYPE_INT, object->mosaicInfo.cellSz,
                          "decim", G_TYPE_INT, object->mosaicInfo.decim,
                          NULL);
    return s;
}
//it's used for do operations together
static void mosaic_render_submit(GapiObject *apiobject, gpointer prims_pointer)
{
    g_assert(apiobject != NULL);
    g_assert(prims_pointer != NULL);
    GApiObjectMosaic *object = G_API_OBJECT_MOSAIC(apiobject);
    ((cv::gapi::wip::draw::Prims *)prims_pointer)->emplace_back(cv::gapi::wip::draw::Mosaic(object->mosaicInfo));
    return;
}
//it's used for do operations one by one
static gboolean mosaic_render_ip(GapiObject *apiobject, GstVideoInfo *sink_info,
                               GstVideoInfo *src_info, GstBuffer *buf)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && buf != NULL), FALSE);
    GApiObjectMosaic *object = G_API_OBJECT_MOSAIC(apiobject);
    return TRUE;
}
//it's used for video scale,video crop, video format convert, or just don't want to modify the origin buffer
static gboolean mosaic_render(GapiObject *apiobject, GstBuffer *in_buf,
                            GstBuffer *out_buf)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && in_buf != NULL
                        && out_buf != NULL), FALSE);
    GApiObjectMosaic *object = G_API_OBJECT_MOSAIC(apiobject);
    return TRUE;
}

GapiObject *gapiobjectMosaic_create()
{
    return (GapiObject *)g_object_new(g_api_object_mosaic_get_type(), NULL);
}
