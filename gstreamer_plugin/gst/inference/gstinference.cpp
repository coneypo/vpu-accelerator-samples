/**
 * SECTION:element-inference
 *
 * Description: This a inference result receiver element, which receive and parse outer results via IPC
 * and pass them to the downstream element.
 */

#include "ipc.h"
#include <atomic>
#include <boost/algorithm/string.hpp>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
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

using namespace HddlUnite;

GST_DEBUG_CATEGORY_STATIC(gst_inference_debug);
#define GST_CAT_DEFAULT gst_inference_debug

enum {
    PROP_0,
    PROP_SILENT,
    PROP_SOCKET_NAME,
    PROP_SYNC_MODE
};

#define GST_TYPE_INFERENCE_SYNC_MODE (gst_inference_sync_mode_get_type())

static GType gst_inference_sync_mode_get_type(void)
{
    static GType inference_sync_mode_type = 0;
    static const GEnumValue mode_types[] = {
        { SYNC_MODE_PTS, "sync with buffer pts", "pts" },
        { SYNC_MODE_INDEX, "sync with frame index", "index" },
        { SYNC_MODE_NOSYNC, "no sync", "nosync" },
        { 0, NULL, NULL }
    };
    if (!inference_sync_mode_type) {
        inference_sync_mode_type = g_enum_register_static("GstInferenceSyncMode", mode_types);
    }
    return inference_sync_mode_type;
}

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
static std::mutex mutex_connection;
static std::condition_variable connection_establised;
static std::condition_variable infer_data_arrived;

static PTS frameIdx = 0;

static bool deserialize(const std::string& serialized_data);
static int receiveRoutine(const char* socket_address);
static int addMetaData(GstInference* infer, GstBuffer* buf);

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

    g_object_class_install_property(gobject_class, PROP_SYNC_MODE,
        g_param_spec_enum("syncmode", "SyncMode", "unix socket file name", GST_TYPE_INFERENCE_SYNC_MODE,
            SYNC_MODE_PTS, GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

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
    case PROP_SYNC_MODE:
        filter->syncmode = (GstInferenceSyncMode)g_value_get_enum(value);
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
    case PROP_SYNC_MODE:
        g_value_set_enum(value, filter->syncmode);
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
            std::unique_lock<std::mutex> connection_lock(mutex_connection);
            return connection_establised.wait_for(connection_lock, std::chrono::milliseconds(10000)) == std::cv_status::no_timeout;
        }();
        if (!connect_client) {
            GST_WARNING("connect to client failed, yet continue playing...");
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

    addMetaData(filter, buf);
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
    const int element_nums = 9;
    std::vector<std::string> fields;
    boost::split(fields, serialized_data, boost::is_any_of(","));
    if (!fields.empty()) {
        fields.pop_back();
    }
    if (fields.size() < element_nums || fields.size() % element_nums != 0) {
        return false;
    }

    gint boxNum = static_cast<gint>(fields.size() / element_nums);
    guint64 pts;
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
        box.inferfps = std::stof(fields[begin_index + 7]);
        box.decfps = std::stof(fields[begin_index + 8]);
        pts = box.pts;
        if (box.height * box.width > 0) {
            boxes.push_back(std::move(box));
        }
    }

    std::lock_guard<std::mutex> lock(mutex_total_results);
    total_results.insert(std::make_pair(pts, boxes));
    infer_data_arrived.notify_all();
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
        case Event::Type::CONNECTION_IN: {
            GST_DEBUG("connection in");
            connection->accept();
            connection_establised.notify_one();
            BoxWrappers box;
            total_results.insert(std::make_pair(0, box));
            break;
        }
        case Event::Type::MESSAGE_IN: {
            int length = 0;
            auto& data_connection = event.connection;
            std::lock_guard<std::mutex> autoLock(data_connection->getMutex());

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
        case Event::Type::CONNECTION_OUT: {
            std::lock_guard<std::mutex> lock(mutex_total_results);
            total_results.clear();
            break;
        }

        default:
            break;
        }
    }
}

static int addMetaData(GstInference* infer, GstBuffer* buf)
{
    PTS boxIndex = 0;
    switch (infer->syncmode) {
    case SYNC_MODE_NOSYNC: {
        std::unique_lock<std::mutex> lock(mutex_total_results);
        auto result = std::find_if(total_results.rbegin(), total_results.rbegin(), [](std::pair<PTS, BoxWrappers> entry) -> bool {
            return !entry.second.empty();
        });
        if (result != total_results.rend()) {
            boxIndex = result->first;
        }
        break;
    }
    case SYNC_MODE_INDEX: {
        boxIndex = frameIdx++;
        break;
    }
    case SYNC_MODE_PTS: {
        boxIndex = GST_BUFFER_PTS(buf);
        break;
    }
    default:
        GST_ERROR("unsupported sync mode");
    }

    std::map<PTS, BoxWrappers>::iterator current_frame_result;
    std::unique_lock<std::mutex> lock(mutex_total_results);
    if (infer_data_arrived.wait_for(lock, std::chrono::milliseconds(1000), [&]() {
        current_frame_result = total_results.find(boxIndex);
        return current_frame_result != total_results.end(); })) {
        size_t boxNums = current_frame_result->second.size();
        InferResultMeta* meta = gst_buffer_add_infer_result_meta(buf, boxNums);
        for (size_t i = 0; i < boxNums; i++) {
            meta->boundingBox[i].x = current_frame_result->second[i].x;
            meta->boundingBox[i].y = current_frame_result->second[i].y;
            meta->boundingBox[i].pts = current_frame_result->second[i].pts;
            meta->boundingBox[i].width = current_frame_result->second[i].width;
            meta->boundingBox[i].height = current_frame_result->second[i].height;
            meta->boundingBox[i].inferfps = current_frame_result->second[i].inferfps;
            meta->boundingBox[i].decfps = current_frame_result->second[i].decfps;
            strncpy(meta->boundingBox[i].label, current_frame_result->second[i].label_str.c_str(), MAX_STR_LEN);
        }
        if (current_frame_result->first > 0) {
            total_results.erase(total_results.upper_bound(0), current_frame_result);
        }
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
