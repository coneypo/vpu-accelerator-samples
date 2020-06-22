/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "gapiobjectimage.h"
#include <opencv2/imgcodecs/legacy/constants_c.h>
#define g_api_object_image_parent_class parent_class
G_DEFINE_TYPE(GApiObjectImage, g_api_object_image, G_TYPE_API_OJBECT);

static gboolean image_parse_json(GapiObject *apiobject,
                                json_object *json_object);
static gboolean image_parse_gst_structure(GapiObject *apiobject,
        GstStructure *structure);
static GstStructure *image_to_gst_structure(GapiObject *apiobject);
//it's used for do operations together
static void image_render_submit(GapiObject *apiobject, gpointer prims_pointer);
//it's used for do operations one by one
static gboolean image_render_ip(GapiObject *apiobject, GstVideoInfo *sink_info,
                               GstVideoInfo *src_info, GstBuffer *buf);
static gboolean image_render(GapiObject *apiobject, GstBuffer *in_buf,
                            GstBuffer *out_buf);

/* initialize the plugin's class */
static void
g_api_object_image_class_init(GApiObjectImageClass *klass)
{
    GapiObjectClass *gapi_object_class;
    gapi_object_class = (GapiObjectClass *) klass;
    gapi_object_class->parse_json = image_parse_json;
    gapi_object_class->parse_gst_structure = image_parse_gst_structure;
    gapi_object_class->to_gst_structure = image_to_gst_structure;
    gapi_object_class->render_submit = image_render_submit;
    gapi_object_class->render_ip = image_render_ip;
    gapi_object_class->render = image_render;
}

/* initialize the new element
 * initialize instance structure
 */
static void
g_api_object_image_init(GApiObjectImage *object)
{
    //the default option in case of playing image failed
    object->imageInfo.org.x = 300;
    object->imageInfo.org.y = 300;

    object->img_path = "";
//    object->alpha_val = 100;

}

/*json_design:*/
/*{*/
/*"meta_id":6,*/
/*"meta_type":"image",*/
/*"img_path":"the diretory of image",*/ 
/*"alpha_val":"value of alpha channel",*/
/*}*/
static gboolean image_parse_json(GapiObject *apiobject,
                                json_object *json_object)
{
    g_assert(apiobject != NULL);
    g_assert(json_object != NULL);

    GApiObjectImage *object = G_API_OBJECT_IMAGE(apiobject);
    const char *meta_type_str = gapiosd_json_get_string(json_object, "meta_type");
    if (meta_type_str == NULL) {
        return FALSE;
    }
    if (std::string(meta_type_str) != std::string("image")) {
        GST_ERROR_OBJECT(object, "json_object with wrong meta_type \n");
        return FALSE;
    }
    gapiosd_json_get_int(json_object, "x", &(object->imageInfo.org.x));
    gapiosd_json_get_int(json_object, "y", &(object->imageInfo.org.y));
    gapiosd_json_get_int(json_object, "alpha", &(object->alpha_val));


    const char* tmp_path = "";
    tmp_path = gapiosd_json_get_string(json_object, "img_path");
    if(g_file_test(tmp_path,G_FILE_TEST_IS_REGULAR)){
         object->img_path = tmp_path;
    }else{
        GST_WARNING_OBJECT(object, "WARNING : The image file(%s) is not exist\n", tmp_path);
        return FALSE;
    }

    object->imageInfo.img = cv::imread(object->img_path,CV_LOAD_IMAGE_UNCHANGED);
    if(object->imageInfo.img.empty()){
        GST_WARNING_OBJECT(object, "WARNING : Read image file(%s) error\n", tmp_path);
        return FALSE;
    }
    if(object->alpha_val < 1){
        object->alpha_val = 1 ;
    }else if(object->alpha_val >255){
        object->alpha_val = 255 ;
    }
    float GrayScale = (float)(object->alpha_val)/255;
    object->imageInfo.alpha = cv::Mat(object->imageInfo.img.rows, object->imageInfo.img.cols, CV_32FC1,cv::Scalar::all(GrayScale));

    return G_API_OBJECT_CLASS(parent_class)->parse_json(apiobject, json_object);
}
/*structure_design:*/
/*typedef struct Image*/
/*{*/
/*uint "meta_id"*/
/*string "meta_type"*/
/*int "x"*/
/*int "y"*/
/*gchar* "img_path"*/
/*int "alpha"*/
/*}Image;*/
static gboolean image_parse_gst_structure(GapiObject *apiobject,
        GstStructure *structure)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && structure != NULL), FALSE);
    GApiObjectImage *object = G_API_OBJECT_IMAGE(apiobject);
    if (!gst_structure_has_name(structure, "gapiosd_meta")) {
        GST_ERROR_OBJECT(object, "GstStructure with a wrong name !\n");
        return FALSE;
    }
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "x",
                       &(object->imageInfo.org.x)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "y",
                       &(object->imageInfo.org.y)), FALSE);
    RETURN_VAL_IF_FAIL(gst_structure_get_int(structure, "alpha", &(object->alpha_val)), FALSE);
    const char* tmp_path = "";
    tmp_path = gst_structure_get_string(structure, "img_path");
    if(g_file_test(tmp_path,G_FILE_TEST_IS_REGULAR)){
         object->img_path = tmp_path;
    }else{
        GST_WARNING_OBJECT(object, "WARNING : The image file(%s) is not exist\n", tmp_path);
        return FALSE;
    }
    object->imageInfo.img = cv::imread(object->img_path,CV_LOAD_IMAGE_UNCHANGED);
    if(object->imageInfo.img.empty()){
        GST_WARNING_OBJECT(object, "WARNING : Read image file(%s) error\n", tmp_path);
        return FALSE;
    }
    if(object->alpha_val < 1){
        object->alpha_val = 1 ;
    }else if(object->alpha_val >255){
        object->alpha_val = 255 ;
    }
    float GrayScale = (float)(object->alpha_val)/255;
    object->imageInfo.alpha = cv::Mat(object->imageInfo.img.rows, object->imageInfo.img.cols, CV_32FC1,cv::Scalar::all(GrayScale));

    return G_API_OBJECT_CLASS(parent_class)->parse_gst_structure(apiobject, structure);
}

static GstStructure *image_to_gst_structure(GapiObject *apiobject)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL), NULL);
    GApiObjectImage *object = G_API_OBJECT_IMAGE(apiobject);
    GstStructure *s =
        gst_structure_new("gapiosd_meta",
                          "meta_id", G_TYPE_UINT, apiobject->meta_id,
                          "meta_type", G_TYPE_STRING, apiobject->meta_type,
                          "x", G_TYPE_INT, object->imageInfo.org.x,
                          "y", G_TYPE_INT, object->imageInfo.org.y,
                          "img_path", G_TYPE_STRING, object->img_path,
                          "alpha", G_TYPE_INT, object->alpha_val,
                          NULL);
    return s;
}
//it's used for do operations together
static void image_render_submit(GapiObject *apiobject, gpointer prims_pointer)
{
    g_assert(apiobject != NULL);
    g_assert(prims_pointer != NULL);
    GApiObjectImage *object = G_API_OBJECT_IMAGE(apiobject);
    ((cv::gapi::wip::draw::Prims *)prims_pointer)->emplace_back(cv::gapi::wip::draw::Image(object->imageInfo));
    return;
}
//it's used for do operations one by one
static gboolean image_render_ip(GapiObject *apiobject, GstVideoInfo *sink_info,
                               GstVideoInfo *src_info, GstBuffer *buf)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && buf != NULL), FALSE);
    GApiObjectImage *object = G_API_OBJECT_IMAGE(apiobject);
    return TRUE;
}
static gboolean image_render(GapiObject *apiobject, GstBuffer *in_buf,
                            GstBuffer *out_buf)
{
    RETURN_VAL_IF_FAIL((apiobject != NULL && in_buf != NULL
                        && out_buf != NULL), FALSE);
    GApiObjectImage *object = G_API_OBJECT_IMAGE(apiobject);
    return TRUE;
}

GapiObject *gapiobjectImage_create()
{
    return (GapiObject *)g_object_new(g_api_object_image_get_type(), NULL);
}
