/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "../gst-libs/gapiobjecttext.h"
#include "../gst/gstgapiosd.h"
#include "../gst-libs/gapiobjectrectangle.h"
#include <opencv2/imgcodecs.hpp>
#include <gst/check/gstcheck.h>

json_object *parse_jsonlist(const char *filename)
{
  json_object * json_objects = json_object_from_file(filename);
  return json_objects;
}

gboolean  setformat(GstVideoInfo *src_info, guint width, guint height, const char *formats)
{
  gst_video_info_init(src_info);
  if((std::string(formats) == std::string("BGR")) || (std::string(formats) == std::string("bgr"))) {
    gst_video_info_set_format(src_info, GST_VIDEO_FORMAT_BGR, width, height);
  }
  if((std::string(formats) == std::string("NV12")) || (std::string(formats) == std::string("nv12"))) {
    gst_video_info_set_format(src_info, GST_VIDEO_FORMAT_NV12, width, height);
  }
  return TRUE;
}

gboolean rend_set_save(json_object *json_objects, gint width, gint height, const char *preimage, const char *formats)
{
  GstStructure *str;
  GstMapInfo map;
  guint size = 0;
  GapiObject *apiobject = gapiobjectText_create();
  fail_if(apiobject == NULL, "apiobject create failed");
  GapiObjectClass *objectclass = G_API_OBJECT_TO_CLASS(apiobject);
  fail_if(objectclass == NULL, "objectclass create failed");
  gboolean ret_js = objectclass->parse_json(apiobject, json_objects);
  fail_if(ret_js == false, "parse json failed");
  str = objectclass->to_gst_structure(apiobject);
  fail_if(str == NULL, "to gst structure failed");
  gboolean ret_stru = objectclass->parse_gst_structure(apiobject, str);
  fail_if(ret_stru == false, "parse gst structure failed");
  gpointer prims_pointer = init_array();
  fail_if(prims_pointer == NULL, "init array failed");
  objectclass->render_submit(apiobject, prims_pointer);
  if(std::string(formats) == std::string("BGR")) {
    size = width * height * 3;
  }
  if(std::string(formats) == std::string("NV12")) {
    size = width * height * 3/2;
  }
  char *memory = g_new0(char, size);
  memset(memory, 128, size);
  GstBuffer* in_buf = gst_buffer_new_wrapped (memory, size);
  GstVideoInfo *sink_info = gst_video_info_new();
  setformat(sink_info, width, height, formats);
  GstVideoInfo *src_info = gst_video_info_new();
  setformat(src_info, width, height, formats);
  gboolean rend = render_sync(in_buf, sink_info, src_info, prims_pointer);
  fail_if(rend == false, "render sync failed");
  gboolean gbm = gst_buffer_map(in_buf, &map, GST_MAP_READ);
  fail_if(gbm == false, "gst buffer map failed");
  cv::Mat save_in_mat;
  if (GST_VIDEO_INFO_FORMAT(src_info) == GST_VIDEO_FORMAT_BGR) {
    save_in_mat = cv::Mat(height, width, CV_8UC3, map.data);
  }
  if (GST_VIDEO_INFO_FORMAT(src_info) == GST_VIDEO_FORMAT_NV12) {
    save_in_mat = cv::Mat(height*3/2, width, CV_8UC1, map.data);
  }
  cv::Mat image_mat;
  if (std::string(formats) == std::string("BGR")) {
    image_mat = save_in_mat;
  }
  if (std::string(formats) == std::string("NV12")) {
    cv::cvtColor(save_in_mat, image_mat, cv::COLOR_YUV2BGR_NV12);
  }
  char image_name[25];
  snprintf(image_name, 24, "%s%s", preimage, ".jpg");
  cv::imwrite(image_name, image_mat);
  gst_buffer_unmap(in_buf, &map);
  json_object_put(json_objects);
  g_object_unref(apiobject);
  gst_structure_free(str);
  gst_video_info_free(sink_info);
  gst_video_info_free(src_info);
  gst_buffer_unref(in_buf);
  return TRUE;
}

gboolean rend_set_save_rect(json_object *json_objects, gint width, gint height, const char *preimage, const char *formats)
{
  GstStructure *str;
  guint size = 0;
  GstMapInfo map;
  GapiObject *apiobject = gapiobjectRect_create();
  fail_if(apiobject == NULL, "apiobject create failed");
  GapiObjectClass *objectclass = G_API_OBJECT_TO_CLASS(apiobject);
  fail_if(objectclass == NULL, "objectclass create failed");
  gboolean ret_js = objectclass->parse_json(apiobject, json_objects);
  fail_if(ret_js == false, "parse json failed");
  str = objectclass->to_gst_structure(apiobject);
  fail_if(str == NULL, "to gst structure failed");
  gboolean ret_stru = objectclass->parse_gst_structure(apiobject, str);
  fail_if(ret_stru == false, "parse gst structure failed");
  gpointer prims_pointer = init_array();
  fail_if(prims_pointer == NULL, "init array failed");
  objectclass->render_submit(apiobject, prims_pointer);
  if(std::string(formats) == std::string("BGR")) {
    size = width * height * 3;
  }
  if(std::string(formats) == std::string("NV12")) {
    size = width * height * 3/2;
  }
  char *memory = g_new0(char, size);
  memset(memory, 128, size);
  GstBuffer* in_buf = gst_buffer_new_wrapped (memory, size);
  GstVideoInfo *sink_info = gst_video_info_new();
  setformat(sink_info, width, height, formats);
  GstVideoInfo *src_info = gst_video_info_new();
  setformat(src_info, width, height, formats);
  gboolean rend = render_sync(in_buf, sink_info, src_info, prims_pointer);
  fail_if(rend == false, "render sync failed");
  gboolean gbm = gst_buffer_map(in_buf, &map, GST_MAP_READ);
  fail_if(gbm == false, "gst buffer map failed");
  cv::Mat save_in_mat;
  if (GST_VIDEO_INFO_FORMAT(src_info) == GST_VIDEO_FORMAT_BGR) {
    save_in_mat = cv::Mat(height, width, CV_8UC3, map.data);
  }
  if (GST_VIDEO_INFO_FORMAT(src_info) == GST_VIDEO_FORMAT_NV12) {
    save_in_mat = cv::Mat(height*3/2, width, CV_8UC1, map.data);
  }
  cv::Mat image_mat;
  if(std::string(formats) == std::string("BGR")) {
    image_mat = save_in_mat;
  }
  if(std::string(formats) == std::string("NV12")) {
    cv::cvtColor(save_in_mat, image_mat, cv::COLOR_YUV2BGR_NV12);
  }
  char image_name[25];
  snprintf(image_name, 24, "%s%s", preimage, ".jpg");
  cv::imwrite(image_name, image_mat);
  gst_buffer_unmap(in_buf, &map);
  json_object_put(json_objects);
  g_object_unref(apiobject);
  gst_video_info_free(sink_info);
  gst_video_info_free(src_info);
  gst_buffer_unref(in_buf);
  return TRUE;
}

GST_START_TEST (test_text_case1)
{
  json_object *json_objects;
  gint width = 1920;
  gint height = 1080;
  json_objects = parse_jsonlist("osd_config_text1.json");
  const char *formats = "BGR";
  gboolean brss = rend_set_save(json_objects, width, height, "testcaseTextBGR", formats);
  fail_if(brss == false, "parse json testcaseTextBGR failed");
}
GST_END_TEST;

GST_START_TEST (test_text_case2)
{
  json_object *json_objects;
  gint width = 1920;
  gint height = 1080;
  json_objects = parse_jsonlist("osd_config_text2.json");
  const char *formats = "NV12";
  gboolean brss = rend_set_save(json_objects, width, height, "testcaseTextNV12", formats);
  fail_if(brss == false, "parse json testcaseTextNV12 failed");
}
GST_END_TEST;

GST_START_TEST (test_text_case3)
{
  json_object *json_objects;
  gint width = 1280;
  gint height = 720;
  json_objects = parse_jsonlist("osd_config_text1.json");
  const char *formats = "BGR";
  gboolean brss = rend_set_save(json_objects, width, height, "testcaseTextBGR720", formats);
  fail_if(brss == false, "parse json testcaseTextBGR failed");
}
GST_END_TEST;

GST_START_TEST (test_text_case4)
{
  json_object *json_objects;
  gint width = 1280;
  gint height = 720;
  json_objects = parse_jsonlist("osd_config_text2.json");
  const char *formats = "NV12";
  gboolean brss = rend_set_save(json_objects, width, height, "testcaseTextNV12720", formats);
  fail_if(brss == false, "parse json testcaseTextNV12 failed");
}
GST_END_TEST;

GST_START_TEST (test_text_case5)
{
  json_object *json_objects;
  gint width = 640;
  gint height = 480;
  json_objects = parse_jsonlist("osd_config_text1.json");
  const char *formats = "BGR";
  gboolean brss = rend_set_save(json_objects, width, height, "testcaseTextBGR480", formats);
  fail_if(brss == false, "parse json testcaseTextBGR failed");
}
GST_END_TEST;

GST_START_TEST (test_text_case6)
{
  json_object *json_objects;
  gint width = 640;
  gint height = 480;
  json_objects = parse_jsonlist("osd_config_text2.json");
  const char *formats = "NV12";
  gboolean brss = rend_set_save(json_objects, width, height, "testcaseTextNV12480", formats);
  fail_if(brss == false, "parse json testcaseTextNV12 failed");
}
GST_END_TEST;

GST_START_TEST (test_rect_case1)
{
  json_object *json_objects;
  gint width = 1920;
  gint height = 1080;
  json_objects = parse_jsonlist("osd_config_rect1.json");
  const char *formats = "NV12";
  gboolean brss = rend_set_save_rect(json_objects, width, height, "testcaseRectNV12", formats);
  fail_if(brss == false, "parse json testcaseRectNV12 failed");
}
GST_END_TEST;

GST_START_TEST (test_rect_case2)
{
  json_object *json_objects;
  gint width = 1920;
  gint height = 1080;
  json_objects = parse_jsonlist("osd_config_rect2.json");
  const char *formats = "BGR";
  gboolean brss = rend_set_save_rect(json_objects, width, height, "testcaseRectBGR", formats);
  fail_if(brss == false, "parse json testcaseRectBGR failed");
}
GST_END_TEST;

GST_START_TEST (test_rect_case3)
{
  json_object *json_objects;
  gint width = 1280;
  gint height = 720;
  json_objects = parse_jsonlist("osd_config_rect1.json");
  const char *formats = "BGR";
  gboolean brss = rend_set_save_rect(json_objects, width, height, "testcaseRectBGR720", formats);
  fail_if(brss == false, "parse json testcaseRectBGR failed");
}
GST_END_TEST;

GST_START_TEST (test_rect_case4)
{
  json_object *json_objects;
  gint width = 1280;
  gint height = 720;
  json_objects = parse_jsonlist("osd_config_rect2.json");
  const char *formats = "NV12";
  gboolean brss = rend_set_save_rect(json_objects, width, height, "testcaseRectNV12720", formats);
  fail_if(brss == false, "parse json testcaseRectNV12 failed");
}
GST_END_TEST;

GST_START_TEST (test_rect_case5)
{
  json_object *json_objects;
  gint width = 640;
  gint height = 480;
  json_objects = parse_jsonlist("osd_config_rect1.json");
  const char *formats = "BGR";
  gboolean brss = rend_set_save_rect(json_objects, width, height, "testcaseRectBGR480", formats);
  fail_if(brss == false, "parse json testcaseRectBGR failed");
}
GST_END_TEST;

GST_START_TEST (test_rect_case6)
{
  json_object *json_objects;
  gint width = 640;
  gint height = 480;
  json_objects = parse_jsonlist("osd_config_rect2.json");
  const char *formats = "NV12";
  gboolean brss = rend_set_save_rect(json_objects, width, height, "testcaseRectNV12480", formats);
  fail_if(brss == false, "parse json testcaseRectNV12 failed");
}
GST_END_TEST;

static Suite *
gst_gapiobjecttext_suite (void)
{
  Suite *s = suite_create ("Gapiobjecttext");
  TCase *tc_chain = tcase_create ("objecttext");
  tcase_set_timeout (tc_chain, 20);
  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_text_case1);
  tcase_add_test (tc_chain, test_text_case2);
  tcase_add_test (tc_chain, test_text_case3);
  tcase_add_test (tc_chain, test_text_case4);
  tcase_add_test (tc_chain, test_text_case5);
  tcase_add_test (tc_chain, test_text_case6);
  tcase_add_test (tc_chain, test_rect_case1);
  tcase_add_test (tc_chain, test_rect_case2);
  tcase_add_test (tc_chain, test_rect_case3);
  tcase_add_test (tc_chain, test_rect_case4);
  tcase_add_test (tc_chain, test_rect_case5);
  tcase_add_test (tc_chain, test_rect_case6);
  return s;
}
int main (int argc, char **argv)
{
  gst_init(&argc,&argv);
  Suite *s;
  gst_check_init (&argc, &argv);
  s = gst_gapiobjecttext_suite ();
  gst_check_run_suite (s, "gapiobjecttext", __FILE__);
  return 0;
}
