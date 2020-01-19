#include <gst/gst.h>

#define VIDEO_FILE "/home/xiao/1600x1200.mp4"

static void cb_on_new_pad(GstElement *element, GstPad *pad, gpointer data);
static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data);

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

int main(int argc, char *argv[]) { return run_pipeline3(argc, argv); }
