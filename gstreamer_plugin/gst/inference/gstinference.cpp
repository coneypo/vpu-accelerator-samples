/**
 * SECTION:element-inference
 *
 * Description: This a inference result receiver element, which receive and parse outer results via IPC
 * and pass them to the downstream element.
 */

#include "IPC.h"
#include "Semaphore.h"
#include <atomic>
#include <boost/algorithm/string.hpp>
#include <map>
#include <string>
#include <thread>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstinference.h"
#include "inferresultmeta.h"
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define pts_sync 0
#define use_local_result 0
using namespace HddlUnite;

GST_DEBUG_CATEGORY_STATIC(gst_inference_debug);
#define GST_CAT_DEFAULT gst_inference_debug

enum {
    PROP_0,
    PROP_SILENT,
    PROP_SOCKET_NAME
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("ANY"));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("ANY"));

#define gst_inference_parent_class parent_class
G_DEFINE_TYPE(GstInference, gst_inference, GST_TYPE_ELEMENT);

struct BoxWrapper : public BoundingBox {
    std::string label_str;
};
typedef std::vector<BoxWrapper> BoxWrappers;
typedef guint64 PTS;
static std::map<PTS, BoxWrappers> total_results;

static std::mutex mutex_total_results;
static Semaphore connection_establised;

static bool deserialize(const std::string& serialized_data);
static int receiveRoutine(const char* socket_address);
static int addMetaData(GstBuffer* buf);

static void gst_inference_set_property(GObject* object, guint prop_id,
    const GValue* value, GParamSpec* pspec);
static void gst_inference_get_property(GObject* object, guint prop_id,
    GValue* value, GParamSpec* pspec);

static gboolean gst_inference_sink_event(GstPad* pad, GstObject* parent, GstEvent* event);
static GstFlowReturn gst_inference_chain(GstPad* pad, GstObject* parent, GstBuffer* buf);

/* GObject vmethod implementations */

/* initialize the inference's class */
static void
gst_inference_class_init(GstInferenceClass* klass)
{
    GObjectClass* gobject_class;
    GstElementClass* gstelement_class;

    gobject_class = (GObjectClass*)klass;
    gstelement_class = (GstElementClass*)klass;

    gobject_class->set_property = gst_inference_set_property;
    gobject_class->get_property = gst_inference_get_property;

    g_object_class_install_property(gobject_class, PROP_SILENT,
        g_param_spec_boolean("silent", "Silent", "Produce verbose output ?",
            FALSE, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_SOCKET_NAME,
        g_param_spec_string("socketname", "SocketName", "unix socket file name",
            "/var/tmp/gstreamer_ipc.sock", G_PARAM_READWRITE));

    gst_element_class_set_details_simple(gstelement_class,
        "Inference",
        "Generic",
        "Generic Template Element",
        "Lu, Gaoyong <<gaoyong.lu@intel.com>>");

    gst_element_class_add_pad_template(gstelement_class,
        gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(gstelement_class,
        gst_static_pad_template_get(&sink_factory));
}

static void
gst_inference_init(GstInference* filter)
{
    filter->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_event_function(filter->sinkpad,
        GST_DEBUG_FUNCPTR(gst_inference_sink_event));
    gst_pad_set_chain_function(filter->sinkpad,
        GST_DEBUG_FUNCPTR(gst_inference_chain));
    GST_PAD_SET_PROXY_CAPS(filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);

    filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    GST_PAD_SET_PROXY_CAPS(filter->srcpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    filter->silent = FALSE;
    filter->sockname = "/var/tmp/gstreamer_ipc.sock";
}

static void
gst_inference_set_property(GObject* object, guint prop_id,
    const GValue* value, GParamSpec* pspec)
{
    GstInference* filter = GST_INFERENCE(object);

    switch (prop_id) {
    case PROP_SILENT:
        filter->silent = g_value_get_boolean(value);
        break;
    case PROP_SOCKET_NAME:
        filter->sockname = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gst_inference_get_property(GObject* object, guint prop_id,
    GValue* value, GParamSpec* pspec)
{
    GstInference* filter = GST_INFERENCE(object);

    switch (prop_id) {
    case PROP_SILENT:
        g_value_set_boolean(value, filter->silent);
        break;
    case PROP_SOCKET_NAME:
        g_value_set_string(value, filter->sockname);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_inference_sink_event(GstPad* pad, GstObject* parent, GstEvent* event)
{
    GstInference* filter;
    gboolean ret;

    filter = GST_INFERENCE(parent);

    GST_LOG_OBJECT(filter, "Received %s event: %" GST_PTR_FORMAT,
        GST_EVENT_TYPE_NAME(event), event);

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_CAPS: {
        GstCaps* caps;
        gst_event_parse_caps(event, &caps);
        ret = gst_pad_event_default(pad, parent, event);
        break;
    }
    case GST_EVENT_STREAM_START: {
        GST_DEBUG("server is waiting connection .....");
        //this function will block until socket connection is established.
        static bool connect_client = [&parent]() {
            std::thread(receiveRoutine, GST_INFERENCE(parent)->sockname).detach();
            return connection_establised.waitFor(10000);
        }();
        if (!connect_client) {
            GST_WARNING("connect to client failed, yet continue playing...");
            g_print("connect to client failed, yet continue playing...\n");
        }
    }
    default:
        ret = gst_pad_event_default(pad, parent, event);
        break;
    }
    return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_inference_chain(GstPad* pad, GstObject* parent, GstBuffer* buf)
{
    GstInference* filter;

    filter = GST_INFERENCE(parent);

#if use_local_result //use fake data
    gint boxNum = 3;
    InferResultMeta* meta = gst_buffer_add_infer_result_meta(buf, boxNum);
    static guint i = 300;
    for (gint j = 0; j < boxNum; j++) {
        meta->boundingBox[j].x = i % 200 + j * 200;
        meta->boundingBox[j].y = i % 200 + j * 250;
        meta->boundingBox[j].width = 200;
        meta->boundingBox[j].height = 300;
        meta->boundingBox[j].probability = 0;
        meta->boundingBox[j].pts = buf->pts;
        char label[10];
        i++;
        snprintf(label, 10, "label %d", j);
        strcpy(meta->boundingBox[j].label, label);
    }

#else //use remote data
    addMetaData(buf);
#endif
    return gst_pad_push(filter->srcpad, buf);
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
inference_init(GstPlugin* inference)
{
    /* debug category for fltering log messages
   *
   * exchange the string 'Template inference' with your description
   */
    GST_DEBUG_CATEGORY_INIT(gst_inference_debug, "inference",
        0, "Template inference");

    return gst_element_register(inference, "inference", GST_RANK_NONE,
        GST_TYPE_INFERENCE);
}

#ifndef PACKAGE
#define PACKAGE "gstinference"
#endif

GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    inference,
    "inference results receiver",
    inference_init,
    "1.0.0",
    "LGPL",
    "gstinference",
    "https://www.intel.com/")

static bool deserialize(const std::string& serialized_data)
{
    const int element_nums = 7;
    std::vector<std::string> fields;
    boost::split(fields, serialized_data, boost::is_any_of(","));
    if (!fields.empty()) {
        fields.pop_back();
    }
    if (fields.size() < element_nums || fields.size() % element_nums != 0) {
        return false;
    }

    gint boxNum = static_cast<gint>(fields.size() / element_nums);
    BoxWrappers boxes;
    for (gint i = 0; i < boxNum; i++) {
        size_t begin_index = static_cast<size_t>(i * element_nums);
        BoxWrapper box;
        box.x = stoi(fields[begin_index + 0]);
        box.y = stoi(fields[begin_index + 1]);
        box.width = stoi(fields[begin_index + 2]);
        box.height = stoi(fields[begin_index + 3]);
        box.label_str = fields[begin_index + 4];
        box.label = nullptr;
        box.pts = std::stoul(fields[begin_index + 5]);
        box.probability = stod(fields[begin_index + 6]);
        boxes.push_back(std::move(box));
    }
    mutex_total_results.lock();
    total_results.insert(std::make_pair(boxes[0].pts, boxes));
    mutex_total_results.unlock();
    return true;
}

static int receiveRoutine(const char* socket_address)
{
    auto poller = Poller::create();
    auto connection = Connection::create(poller);
    if (!connection->listen(socket_address)) {
        GST_ERROR("Error: Create service listening socket failed.\n");
        return -1;
    }

    while (true) {
        auto event = poller->waitEvent(100);
        switch (event.type) {
        case Event::Type::CONNECTION_IN:
            GST_DEBUG("connection in");
            connection->accept();
            connection_establised.post();
            break;
        case Event::Type::MESSAGE_IN: {
            int length = 0;
            auto& data_connection = event.connection;
            AutoMutex autoLock(data_connection->getMutex());

            if (!data_connection->read(&length, sizeof(length))) {
                GST_ERROR("Error: receive message length failed");
                break;
            }

            if (length <= 0) {
                GST_ERROR("Error: invalid message length, length=%d", length);
                break;
            }

            std::string serialized_data(static_cast<size_t>(length), ' ');
            if (!data_connection->read(&serialized_data[0], length)) {
                GST_ERROR("Error: receive message failed, expectLen=%d ", length);
                break;
            }

            if (!deserialize(serialized_data)) {
                GST_ERROR("Error: data format doesn't match.");
                break;
            }
            break;
        }
        case Event::Type::CONNECTION_OUT:
            GST_DEBUG("connection out");
            mutex_total_results.lock();
            total_results.clear();
            mutex_total_results.unlock();
            break;

        default:
            break;
        }
    }
}

static int addMetaData(GstBuffer* buf)
{
    mutex_total_results.lock();
#if pts_sync
    auto current_frame_result = total_results.find(buf->pts);
#else
    auto current_frame_result = total_results.begin();
#endif
    if (current_frame_result != total_results.end()) {
        size_t boxNums = current_frame_result->second.size();
        InferResultMeta* meta = gst_buffer_add_infer_result_meta(buf, boxNums);
        for (size_t i = 0; i < boxNums; i++) {
            meta->boundingBox[i].x = current_frame_result->second[i].x;
            meta->boundingBox[i].y = current_frame_result->second[i].y;
            meta->boundingBox[i].pts = current_frame_result->second[i].pts;
            meta->boundingBox[i].width = current_frame_result->second[i].width;
            meta->boundingBox[i].height = current_frame_result->second[i].height;
            strncpy(meta->boundingBox[i].label, current_frame_result->second[i].label_str.c_str(), MAX_STR_LEN);
        }
        total_results.erase(current_frame_result);
    }
    mutex_total_results.unlock();
    return 0;
}

#ifdef __cplusplus
}
#endif
