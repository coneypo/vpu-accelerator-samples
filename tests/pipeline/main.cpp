#include <gst/gst.h>
#include <gst/video/gstvideometa.h>
#include <stdlib.h>
#include <stdio.h>

#define VIDEO_FILE "/home/xiao/1600x1200.mp4"

static void cb_on_new_pad(GstElement *element, GstPad *pad, gpointer data);
static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data);
static GstFlowReturn new_sample (GstElement *sink, gpointer data);

static void
on_pad_added (GstElement *element,
              GstPad     *pad,
              gpointer    data)
{
  GstPad *sinkpad;
  GstElement *destelement = (GstElement *) data;

  sinkpad = gst_element_get_static_pad (destelement, "sink");

  gst_pad_link (pad, sinkpad);

  gst_object_unref (sinkpad);
}


int run_pipeline1(int argc, char *argv[]) {
  // filesrc ! qtdemux ! h264parser ! remoteoffloadbin .\( ! mfxh264decode !
  // mfxsink \)
  gst_init(&argc, &argv);

  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  GstBus *bus;
  guint bus_watch_id;

  GstElement *source = gst_element_factory_make("filesrc", "source");
  g_object_set(source, "location", VIDEO_FILE, NULL);
  GstElement *demux = gst_element_factory_make("qtdemux", "demux");
  GstElement *remoteoffloadbin =
      gst_element_factory_make("remoteoffloadbin", "rob");
  GstElement *h264parser = gst_element_factory_make("h264parse", "h264parse");
  GstElement *decoder = gst_element_factory_make("mfxh264dec", "decoder");
  GstElement *sink = gst_element_factory_make("mfxsink", "mysink");

  GstElement *pipeline = gst_pipeline_new("pipeline");
  if (!decoder) {
    return -1;
  }

  gst_bin_add_many(GST_BIN(pipeline), source, demux, h264parser,
                   remoteoffloadbin, NULL);
  if (!remoteoffloadbin) {
    return -1;
  }
  gst_bin_add_many(GST_BIN(remoteoffloadbin), decoder, sink, NULL);

  gst_element_link(source, demux);
  gst_element_link(h264parser, decoder);
  gst_element_link(decoder, sink);

  g_signal_connect(demux, "pad-added", G_CALLBACK(cb_on_new_pad), h264parser);

  bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
  gst_object_unref(bus);

  GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    return -1;
  }

  g_main_loop_run(loop);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  g_main_loop_unref(loop);

  return 0;
}
int run_pipeline2(int argc, char *argv[]) {
  // filesrc ! qtdemux ! h264parse ! tee name=t ! queue name=q0 !
  // remoteoffloadbin.\( mfxh264dec ! mfxsink  \)  t. ! queue ! mfxh264dec !
  // mfxsink

  gst_init(&argc, &argv);
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  GstBus *bus;

  GstElement *source = gst_element_factory_make("filesrc", "source");
  g_object_set(source, "location", VIDEO_FILE, NULL);
  GstElement *demux = gst_element_factory_make("qtdemux", "demux");
  GstElement *h264parser = gst_element_factory_make("h264parse", "h264parse");
  GstElement *tee = gst_element_factory_make("tee", "t0");
  GstElement *queue0 = gst_element_factory_make("queue", "queue0");
  GstElement *queue1 = gst_element_factory_make("queue", "queue1");
  GstElement *remoteoffloadbin =
      gst_element_factory_make("remoteoffloadbin", "rob");
  GstElement *decoder0 = gst_element_factory_make("mfxh264dec", "decoder0");
  GstElement *decoder1 = gst_element_factory_make("mfxh264dec", "decoder1");
  GstElement *sink0 = gst_element_factory_make("mfxsink", "mysink0");
  GstElement *sink1 = gst_element_factory_make("mfxsink", "mysink1");

  GstElement *pipeline = gst_pipeline_new("pipeline");

  gst_bin_add_many(GST_BIN(pipeline), source, demux, h264parser, tee,
                   queue1, remoteoffloadbin, decoder1, sink1, NULL);
  gst_bin_add_many(GST_BIN(remoteoffloadbin), queue0, decoder0, sink0, NULL);

  gst_element_link(source, demux);
  gst_element_link(h264parser, tee);
  gst_element_link_pads(tee, "src_0", queue0, "sink");
  gst_element_link_pads(tee, "src_1", queue1, "sink");
  gst_element_link(queue0, decoder0);
  gst_element_link(decoder0, sink0);

  gst_element_link(queue1, decoder1);
  gst_element_link(decoder1, sink1);

  bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  guint bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
  gst_object_unref(bus);
  g_signal_connect(demux, "pad-added", G_CALLBACK(cb_on_new_pad), h264parser);

  GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    return -1;
  }

  g_main_loop_run(loop);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  g_main_loop_unref(loop);

  return 0;
}

int run_pipeline3(int argc, char *argv[]) {
  // filesrc ! qtdemux ! h264parse ! tee name=t ! queue name=q0 !
  // remoteoffloadbin.\( vaapih264dec ! vaapisink \)  t. ! queue ! mfxh264dec !
  // mfxsink

  gst_init(&argc, &argv);
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  GstBus *bus;

  GstElement *source = gst_element_factory_make("filesrc", "source");
  g_object_set(source, "location", VIDEO_FILE, NULL);
  GstElement *demux = gst_element_factory_make("qtdemux", "demux");
  GstElement *h264parser = gst_element_factory_make("h264parse", "h264parse");
  GstElement *tee = gst_element_factory_make("tee", "t0");
  GstElement *queue0 = gst_element_factory_make("queue", "queue0");
  GstElement *queue1 = gst_element_factory_make("queue", "queue1");
  GstElement *remoteoffloadbin =
      gst_element_factory_make("remoteoffloadbin", "rob");
  GstElement *decoder0 = gst_element_factory_make("vaapih264dec", "decoder0");
  GstElement *decoder1 = gst_element_factory_make("mfxh264dec", "decoder1");
  GstElement *sink0 = gst_element_factory_make("vaapisink", "mysink0");
  GstElement *sink1 = gst_element_factory_make("mfxsink", "mysink1");


  GstElement *pipeline = gst_pipeline_new("pipeline");

  gst_bin_add_many(GST_BIN(pipeline), source, demux, h264parser, tee,
                   queue1, remoteoffloadbin, decoder1, sink1, NULL);
  gst_bin_add_many(GST_BIN(remoteoffloadbin), queue0, decoder0, sink0, NULL);

  gst_element_link(source, demux);
  gst_element_link(h264parser, tee);
  gst_element_link_pads(tee, "src_0", queue0, "sink");
  gst_element_link_pads(tee, "src_1", queue1, "sink");
  gst_element_link(queue0, decoder0);
  gst_element_link(decoder0, sink0);

  gst_element_link(queue1, decoder1);
  gst_element_link(decoder1, sink1);

  bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  guint bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
  gst_object_unref(bus);
  g_signal_connect(demux, "pad-added", G_CALLBACK(cb_on_new_pad), h264parser);

  GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    return -1;
  }

  g_main_loop_run(loop);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  g_main_loop_unref(loop);

  return 0;
}


void cb_on_new_pad(GstElement *demux, GstPad *pad, gpointer data) {
  // gchar *name;
  // GstElement* decoder = (GstElement*)data;
  // name = gst_pad_get_name(pad);
  // if(!strcmp(name, "video_0")){
  //    if(!gst_element_link_pads(demux, name,decoder,"sink"))
  //    {
  //        g_printerr("Elements could not be linked \n");
  //    }

  //}
  // g_free(name);
  GstElement *dstElement = (GstElement *)data;
  GstPad *sinkPad = gst_element_get_static_pad(dstElement, "sink");
  GstPad *srcPad = gst_element_get_compatible_pad(demux, sinkPad, NULL);
  gst_pad_link(srcPad, sinkPad);
  gst_object_unref(sinkPad);
  gst_object_unref(srcPad);
}

gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data) {
  GMainLoop *loop = (GMainLoop *)data;

  switch (GST_MESSAGE_TYPE(msg)) {
  case GST_MESSAGE_EOS:
    g_print("End of stream\n");
    g_main_loop_quit(loop);
    break;

  case GST_MESSAGE_ERROR: {
    gchar *debug;
    GError *error;

    gst_message_parse_error(msg, &error, &debug);
    g_free(debug);

    g_printerr("Error: %s\n", error->message);
    g_error_free(error);

    g_main_loop_quit(loop);
    break;
  }
  default:
    break;
  }
  return TRUE;
}
/* The appsink has received a buffer */
static GstFlowReturn new_sample (GstElement *sink, gpointer data) {
  GstSample *sample;

  /* Retrieve the buffer */
  g_signal_emit_by_name (sink, "pull-sample", &sample);
  if (sample) {
    /* The only thing we do in this example is print a * to indicate a received buffer */
    GstBuffer* metaBuffer = gst_sample_get_buffer(sample);
    if(metaBuffer){
      GstClockTime pts = GST_BUFFER_PTS(metaBuffer);

      GstVideoRegionOfInterestMeta *meta_orig = NULL;
      gpointer state = NULL;
      while( (meta_orig = (GstVideoRegionOfInterestMeta *)
          gst_buffer_iterate_meta_filtered(metaBuffer,
                                           &state,
                                           gst_video_region_of_interest_meta_api_get_type())) )
      {
              g_print("x:%d, y:%d, w:%d, h: %d, type: %s \n", meta_orig->x, meta_orig->y, meta_orig->w, meta_orig->h, g_quark_to_string(meta_orig->roi_type));
      }

    }
    gst_sample_unref (sample);
    return GST_FLOW_OK;
  }
  return GST_FLOW_ERROR;
}
int run_pipeline4(int argc, char *argv[]) {
  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  GstBus *bus;
  guint bus_watch_id;
  GMainLoop *loop;

  loop = g_main_loop_new (NULL, FALSE);

  char *videxamplesenv = getenv("VIDEO_EXAMPLES_DIR");
  if( !videxamplesenv )
  {
    fprintf(stderr, "Error! VIDEO_EXAMPLES_DIR env not set.\n");
    fprintf(stderr, " Set to /path/to/video-examples\n");
    return -1;
  }

  char *modelspathenv = getenv("MODELS_PATH");
  if( !modelspathenv )
  {
    fprintf(stderr, "Error! MODELS_PATH env not set.\n");
    return -1;
  }

  char *gvapluginsenv = getenv("GVA_HOME");
  if( !gvapluginsenv )
  {
    gvapluginsenv = getenv("GVA_PLUGINS");
  }
  if( !gvapluginsenv )
  {
    fprintf(stderr, "Error! GVA_HOME or GVA_PLUGINS env not set.\n");
    fprintf(stderr, " Set to /path/to/gstreamer-plugins\n");
    fprintf(stderr, " (which is the directory created from git clone https://github.intel.com/video-analytics/gstreamer-plugins.git\n");
    return -1;
  }

  gchar *videofilename =  g_strdup_printf( "%s/Fun_at_a_Fair.mp4", videxamplesenv);
  gchar *detectionmodelfilename =  g_strdup_printf( "%s/intel/face-detection-adas-0001/FP32/face-detection-adas-0001.xml", modelspathenv);
  gchar *classifymodel0filename =  g_strdup_printf( "%s/intel/age-gender-recognition-retail-0013/FP32/age-gender-recognition-retail-0013.xml", modelspathenv);
  gchar *classifymodel0procfilename =  g_strdup_printf( "%s/samples/model_proc/age-gender-recognition-retail-0013.json", gvapluginsenv);
  gchar *classifymodel1filename =  g_strdup_printf( "%s/intel/emotions-recognition-retail-0003/FP32/emotions-recognition-retail-0003.xml", modelspathenv);
  gchar *classifymodel1procfilename =  g_strdup_printf( "%s/samples/model_proc/emotions-recognition-retail-0003.json", gvapluginsenv);

  fprintf(stderr, "Using video file = %s\n", videofilename);
  fprintf(stderr, "Using detection model file = %s\n", detectionmodelfilename);
  fprintf(stderr, "Using classify 0 model file = %s\n", classifymodel0filename);
  fprintf(stderr, "Using classify 0 model proc file = %s\n", classifymodel0procfilename);
  fprintf(stderr, "Using classify 1 model file = %s\n", classifymodel1filename);
  fprintf(stderr, "Using classify 1 model proc file = %s\n", classifymodel1procfilename);

  GstElement *filesrc = gst_element_factory_make("filesrc", "myfilesrc");
  g_object_set (G_OBJECT (filesrc), "location", videofilename, NULL);
  GstElement *decodebin = gst_element_factory_make("decodebin", "mydecodebin");
  GstElement *capsfilter0 = gst_element_factory_make("capsfilter", "capsfilter0");
  {
    gchar *capsstr;
    capsstr = g_strdup_printf ("video/x-raw");
    GstCaps *caps = gst_caps_from_string (capsstr);
    g_object_set (capsfilter0, "caps", caps, NULL);
    gst_caps_unref (caps);
  }

  GstElement *capsfilter1 = gst_element_factory_make("capsfilter", "capsfilter1");
  {
    gchar *capsstr;
    capsstr = g_strdup_printf ("video/x-raw");
    GstCaps *caps = gst_caps_from_string (capsstr);
    g_object_set (capsfilter1, "caps", caps, NULL);
    gst_caps_unref (caps);
  }

  GstElement *videoconvert0 = gst_element_factory_make("videoconvert", "myvideoconvert0");

  GstElement *queue0 = gst_element_factory_make("queue", "myqueue0");

  GstElement *gvadetect = gst_element_factory_make("gvadetect", "mygvadetect");
  g_object_set (G_OBJECT (gvadetect), "model", detectionmodelfilename, NULL);
  //g_object_set (G_OBJECT (gvainference), "device", "GPU", NULL);

  GstElement *queue1 = gst_element_factory_make("queue", "myqueue1");

  GstElement *gvaclassify0 = gst_element_factory_make("gvaclassify", "mygvaclassify0");
  g_object_set (G_OBJECT (gvaclassify0), "model", classifymodel0filename, NULL);
  g_object_set (G_OBJECT (gvaclassify0), "model-proc", classifymodel0procfilename, NULL);

  GstElement *queue2 = gst_element_factory_make("queue", "myqueue2");

  GstElement *gvaclassify1 = gst_element_factory_make("gvaclassify", "mygvaclassify1");
  g_object_set (G_OBJECT (gvaclassify1), "model", classifymodel1filename, NULL);
  g_object_set (G_OBJECT (gvaclassify1), "model-proc", classifymodel1procfilename, NULL);

  GstElement *queue3 = gst_element_factory_make("queue", "myqueue3");

  GstElement *videoconvert1 = gst_element_factory_make("videoconvert", "myvideoconvert1");
  GstElement *videoconvert2 = gst_element_factory_make("videoconvert", "myvideoconvert2");
  GstElement *videoconvert3 = gst_element_factory_make("videoconvert", "myvideoconvert3");
  GstElement *gvawatermark = gst_element_factory_make("gvawatermark", "mygvawatermark");
  GstElement *videoroicrop = gst_element_factory_make("videoroicrop", "myvideoroicrop");
  if( !videoroicrop )
  {
    fprintf(stderr, "Error creating videocroproi\n");
    return -1;
  }

  GstElement *queue4 = gst_element_factory_make("queue", "myqueue4");
  GstElement *queue5 = gst_element_factory_make("queue", "myqueue5");

  GstElement *videoroicompose = gst_element_factory_make("videoroicompose", "videoroicompose");
  if( !videoroicompose )
  {
    fprintf(stderr, "Error creating videoroicompose\n");
    return -1;
  }

  GstElement *videoroimetadetach = gst_element_factory_make("videoroimetadetach", "myvideoroimetadetach");
  if( !videoroimetadetach )
  {
    fprintf(stderr, "Error creating gvadetectionmetadetach\n");
    return -1;
  }

  GstElement *ximagesinkcomposed = gst_element_factory_make("ximagesink", "ximagesink-composed");
  g_object_set (G_OBJECT (ximagesinkcomposed), "sync", (gboolean)TRUE, NULL);
  g_object_set (G_OBJECT (ximagesinkcomposed), "qos", (gboolean)FALSE, NULL);

  GstElement *remoteoffloadbin = gst_element_factory_make("remoteoffloadbin", "myremoteoffloadbin");
  if( !remoteoffloadbin )
  {
    fprintf(stderr, "Error creating remoteoffloadbin\n");
    return -1;
  }


  GstElement *pipeline = gst_pipeline_new("my_pipeline");
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  gst_bin_add(GST_BIN(pipeline), filesrc);
  gst_bin_add(GST_BIN(pipeline), decodebin);
  gst_bin_add(GST_BIN(pipeline), capsfilter0);
  gst_bin_add(GST_BIN(pipeline), videoconvert0);
  gst_bin_add(GST_BIN(pipeline), queue0);
  gst_bin_add(GST_BIN(remoteoffloadbin), gvadetect);
  gst_bin_add(GST_BIN(remoteoffloadbin), queue1);
  gst_bin_add(GST_BIN(remoteoffloadbin), capsfilter1);
  gst_bin_add(GST_BIN(remoteoffloadbin), videoconvert2);
  gst_bin_add(GST_BIN(remoteoffloadbin), gvaclassify0);
  gst_bin_add(GST_BIN(remoteoffloadbin), queue2);
  gst_bin_add(GST_BIN(remoteoffloadbin), gvaclassify1);
  gst_bin_add(GST_BIN(remoteoffloadbin), queue3);

  gst_bin_add(GST_BIN(remoteoffloadbin), videoroicrop);
  gst_bin_add(GST_BIN(remoteoffloadbin), queue4);
  gst_bin_add(GST_BIN(remoteoffloadbin), queue5);

  gst_bin_add(GST_BIN(remoteoffloadbin), videoconvert3);

  gst_bin_add(GST_BIN(remoteoffloadbin), videoconvert1);
  gst_bin_add(GST_BIN(remoteoffloadbin), videoroicompose);

  gst_bin_add(GST_BIN(pipeline), gvawatermark);
  gst_bin_add(GST_BIN(pipeline), ximagesinkcomposed);
  gst_bin_add(GST_BIN(pipeline), remoteoffloadbin);


  gst_element_link (filesrc, decodebin);
  g_signal_connect (decodebin, "pad-added", G_CALLBACK (on_pad_added), capsfilter0);
  gst_element_link(capsfilter0, videoconvert0);
  gst_element_link(videoconvert0, queue0);
  gst_element_link(queue0, gvadetect);
  gst_element_link(gvadetect, queue1);
  gst_element_link(queue1, capsfilter1);
  gst_element_link(capsfilter1, videoconvert2);
  gst_element_link(videoconvert2, gvaclassify0);
  gst_element_link(gvaclassify0, queue2);
  gst_element_link(queue2, gvaclassify1);
  gst_element_link(gvaclassify1, queue3);
  gst_element_link(queue3, videoroicrop);

  gst_element_link_pads (videoroicrop, "src", queue4, "sink");
  if( !gst_element_link_pads (videoroicrop, "srcmeta", queue5, "sink") )
  {
    fprintf(stderr, "Error linking srcmeta to queue3 sink\n");
  }
  gst_element_link(queue4, videoconvert3);
  gst_element_link_pads (videoconvert3, "src", videoroicompose, "sinkvideo");
  gst_element_link_pads (queue5, "src", videoroicompose, "sinkmeta");
  gst_element_link(videoroicompose, gvawatermark);
  gst_element_link(gvawatermark, ximagesinkcomposed);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  g_main_loop_run (loop);

  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);

  g_free(videofilename);
  g_free(detectionmodelfilename);
  g_free(classifymodel0filename);
  g_free(classifymodel0procfilename);
  g_free(classifymodel1filename);
  g_free(classifymodel1procfilename);

  return 0;
}


int run_pipeline5(){

  gst_init(NULL,NULL);

  gchar* pipelineStr = g_strdup_printf("filesrc location=/home/xiao/1600x1200.mp4 ! qtdemux ! h264parse ! vaapih264dec ! capsfilter caps=video/x-raw  ! videoconvert ! gvadetect model=/home/xiao/gva/data/models/intel/intel/vehicle-license-plate-detection-barrier-0106/FP32/vehicle-license-plate-detection-barrier-0106.xml ! queue ! gvawatermark !  videoroimetadetach ! appsink name=myappsink ");
  //gchar* pipelineStr = g_strdup_printf("filesrc location=/home/xiao/1600x1200.mp4 ! qtdemux ! h264parse ! vaapih264dec ! capsfilter caps=video/x-raw  ! videoconvert ! gvadetect model=/home/xiao/gva/data/models/intel/intel/vehicle-license-plate-detection-barrier-0106/FP32/vehicle-license-plate-detection-barrier-0106.xml ! queue ! gvawatermark !  appsink name=myappsink ");
  //gchar* pipelineStr = g_strdup_printf("videoroimetaattach name=attach ! queue ! gvawatermark ! videoconvert ! fpsdisplaysink filesrc location=/home/xiao/1600x1200.mp4 ! qtdemux ! h264parse ! vaapih264dec ! videoconvert ! gvadetect model=/home/xiao/gva/data/models/intel/intel/vehicle-license-plate-detection-barrier-0106/FP32/vehicle-license-plate-detection-barrier-0106.xml ! tee name=t0 ! queue ! videoroimetadetach ! attach.sinkmeta t0. ! queue ! attach.sinkvideo ");
  //gchar* pipelineStr = g_strdup_printf("videoroimetaattach name=attach ! queue ! gvawatermark ! fpsdisplaysink filesrc location=/home/xiao/1600x1200.mp4 ! qtdemux ! h264parse ! vaapih264dec ! capsfilter caps=video/x-raw ! videoconvert ! gvadetect model=/home/xiao/gva/data/models/intel/intel/vehicle-license-plate-detection-barrier-0106/FP32/vehicle-license-plate-detection-barrier-0106.xml ! tee name=t0 ! queue ! vaapih265enc ! h265parse ! vaapih265dec ! attach.sinkvideo t0. ! queue ! videoroimetadetach ! attach.sinkmeta ");
  //parse gstreamer pipeline
  GstElement* pipeline = gst_parse_launch_full(pipelineStr,  NULL, GST_PARSE_FLAG_FATAL_ERRORS, NULL);
  GMainLoop* loop = g_main_loop_new (NULL, FALSE);
  GstBus* bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  guint bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  GstElement* appSink= gst_bin_get_by_name(GST_BIN(pipeline),"myappsink");
  g_object_set (appSink, "emit-signals", TRUE, NULL);
  g_signal_connect(appSink, "new-sample", G_CALLBACK(new_sample),NULL);


  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_main_loop_run (loop);

  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
  return 0;
}

int main(int argc, char *argv[]) { return run_pipeline5();}
