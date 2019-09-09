/* GStreamer
 *
 * unit test for GstMemory
 *
 * Copyright (C) <2012> Wim Taymans <wim.taymans at gmail.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/* GStreamer
 *
 * unit test for GstMemory
 *
 * Copyright (C) <2012> Wim Taymans <wim.taymans at gmail.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "../gst-libs/gapiobjecttext.h"
#include "../gst/gstapi2d.h"
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
  if((strcmp(formats,"BGR")== 0)||(strcmp(formats,"bgr")== 0))
  {
    gst_video_info_set_format(src_info,GST_VIDEO_FORMAT_BGR,width,height);
  }
  if((strcmp(formats,"NV12")== 0)||(strcmp(formats,"nv12")== 0))
  {
    gst_video_info_set_format(src_info,GST_VIDEO_FORMAT_NV12,width,height);
  }
  return TRUE;
}

gboolean rend_set_save(json_object *json_objects,gint width,gint height,char *preimage,const char *formats)
{
  GstStructure *str;
  GapiObject *apiobject = gapiobjectText_create();
  GapiObjectClass *objectclass = G_API_OBJECT_TO_CLASS(apiobject);
  objectclass->parse_json(apiobject,json_objects);
  str = objectclass->to_gst_structure(apiobject);
  objectclass->parse_gst_structure(apiobject,str);
  GstMapInfo map;
  gpointer prims_pointer = init_array();
  objectclass->render_submit(apiobject,prims_pointer);
  guint size = width * height *3;
  char *memory = g_new0(char,size);
  GstBuffer* in_buf = gst_buffer_new_wrapped (memory ,size);
  GstVideoInfo *sink_info = gst_video_info_new();
  setformat(sink_info,width,height,formats);
  GstVideoInfo *src_info = gst_video_info_new();
  setformat(src_info,width,height,formats);
  render_sync(in_buf, sink_info, src_info, prims_pointer);
  gst_buffer_map(in_buf,&map,GST_MAP_READ);
  cv::Mat save_in_mat;
  if (GST_VIDEO_INFO_FORMAT(src_info) == GST_VIDEO_FORMAT_BGR)
  {
    save_in_mat = cv::Mat(height,width,CV_8UC3,map.data);
  }
  if (GST_VIDEO_INFO_FORMAT(src_info) == GST_VIDEO_FORMAT_NV12)
  {
    save_in_mat = cv::Mat(height,width,CV_8UC1,map.data);
  }
  char image_name[20];
  sprintf(image_name,"%s%s",preimage,".jpg");
  cv::imwrite(image_name,save_in_mat);
  gst_buffer_unmap (in_buf, &map);
  return TRUE;
}

gboolean freeAll(json_object *json_objects)
{
  json_object_put(json_objects);
  return TRUE;
}

GST_START_TEST (test_text_case1)
{
  json_object *json_objects;
  gint width = 1920;
  gint height = 1080;
  json_objects = parse_jsonlist("osd_config_text1.json");
  const char *formats = "BGR";
  gboolean brss = rend_set_save(json_objects,width,height,"testcase1",formats);
  fail_if(brss != TRUE,"parse json testcase1 failed");
  freeAll(json_objects);
}
GST_END_TEST;

GST_START_TEST (test_text_case2)
{
  json_object *json_objects;
  gint width = 1920;
  gint height = 1080;
  json_objects = parse_jsonlist("osd_config_text2.json");
  const char *formats = "NV12";
  gboolean brss = rend_set_save(json_objects,width,height,"testcase2",formats);
  fail_if(brss != TRUE,"parse json testcase2 failed");
  freeAll(json_objects);
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