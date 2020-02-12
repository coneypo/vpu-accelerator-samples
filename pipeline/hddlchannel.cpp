#include "hddlchannel.h"
#include "blockingqueue.h"
#include "cropdefs.h"
#include "fpsstat.h"
#include "socketclient.h"

#include <QApplication>
#include <QDebug>
#include <QHBoxLayout>
#include <QImage>
#include <QTimer>
#include <QWidget>

#include <gst/video/gstvideometa.h>
#include <memory>
#include <opencv2/opencv.hpp>

#define OVERLAY_NAME "mfxsink0"

HddlChannel::HddlChannel(int channelId, QWidget* parent)
    : QMainWindow(parent)
    , m_client(nullptr)
    , m_probPad(nullptr)
    , m_id(channelId)
    , m_stop(false)
{
    QWidget* container = new QWidget(this);
    setCentralWidget(container);
    container->setStyleSheet("background-color: rgb(50, 50, 78);");
    this->resize(QApplication::primaryScreen()->size());

    m_roiThread = std::thread(&HddlChannel::fetchRoiData, this);
}

HddlChannel::~HddlChannel()
{
    if (m_probPad) {
        delete m_probPad;
    }

    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    gst_object_unref(m_pipeline);

    m_stop.store(true);
    m_roiThread.join();
}

bool HddlChannel::setupPipeline(QString pipelineDescription, QString displaySinkName)
{
    // init gstreamer
    gst_init(NULL, NULL);

    //parse gstreamer pipeline
    m_pipeline = gst_parse_launch_full(pipelineDescription.toStdString().c_str(), NULL, GST_PARSE_FLAG_FATAL_ERRORS, NULL);
    GstElement* dspSink = gst_bin_get_by_name(GST_BIN(m_pipeline), displaySinkName.toStdString().c_str());
    Q_ASSERT(dspSink != nullptr);

    GstElement* osdparser = gst_bin_get_by_name(GST_BIN(m_pipeline), "osdparser0");
    Q_ASSERT(osdparser != nullptr);
    g_object_set(osdparser, "roi_queue", &m_roiQueue, NULL);

    //get gstreamer displaysink video overlay
    m_overlay = GST_VIDEO_OVERLAY(gst_bin_get_by_name(GST_BIN(dspSink), OVERLAY_NAME));
    //Q_ASSERT(m_overlay != nullptr);

    GstElement* appSink = gst_bin_get_by_name(GST_BIN(m_pipeline), "myappsink");
    g_object_set(appSink, "emit-signals", TRUE, NULL);
    g_signal_connect(appSink, "new-sample", G_CALLBACK(HddlChannel::new_sample), this);

    GstBus* bus = gst_element_get_bus(m_pipeline);
    gst_bus_add_watch(bus, HddlChannel::busCallBack, this);
    gst_object_unref(bus);


    //probe sinkpad for calculate fps
    GstPad* sinkPad = gst_element_get_static_pad(dspSink, "sink");
    Q_ASSERT(sinkPad != nullptr);
    m_probPad = new FpsStat(sinkPad);

    m_fpstimer = new QTimer(this);
    m_fpstimer->setInterval(1000);
    connect(m_fpstimer, &QTimer::timeout, this, &HddlChannel::sendFps);
    connect(this, &HddlChannel::roiReady, this, &HddlChannel::sendRoiData);

    m_fpstimer->start();
}

bool HddlChannel::initConnection(QString serverPath)
{
    m_client = new SocketClient(serverPath, this);
    if(!m_client->connectServer()){
        return false;
    };
    m_client->sendWinId(this->winId());
    return true;
}

void HddlChannel::run()
{
    QString ipc_name = "/var/tmp/gstreamer_ipc_" + QString::number(m_id);
#ifndef USE_FAKE_RESULT_SENDER
    QTimer::singleShot(200, [this, &ipc_name]() {
        if (!m_sender.connectServer(ipc_name.toStdString())) {
            qDebug() << "connect server error";
        }
    });
#else
    QTimer::singleShot(200, [this, &ipc_name] {
        QProcess* hva_process = new QProcess(this);
        hva_process->setProcessChannelMode(QProcess::ForwardedChannels);
        QString hva_cmd = QString("./fake_result_sender");
        hva_process->start(hva_cmd, QStringList(ipc_name));
    });
#endif

    WId xwinid = this->centralWidget()->winId();
    if(m_overlay) {
        gst_video_overlay_set_window_handle(m_overlay, xwinid);
    }

    GstStateChangeReturn sret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (sret == GST_STATE_CHANGE_FAILURE) {
        QTimer::singleShot(0, QApplication::activeWindow(), SLOT(quit()));
    }
}

void HddlChannel::resizeEvent(QResizeEvent* event)
{
    if(m_overlay) {
        gst_video_overlay_expose(m_overlay);
    }
    QMainWindow::resizeEvent(event);
}

void HddlChannel::sendFps()
{
    auto fps = m_probPad->getRenderFps();
    m_client->sendString(QString::number(fps, 'f', 2));
}

void HddlChannel::fetchRoiData()
{
    while (!m_stop) {
        auto src = m_roiQueue.take();
        cv::Mat image;
        cv::resize(*src, image, cv::Size(CROP_IMAGE_WIDTH, CROP_IMAGE_HEIGHT));
        QByteArray* ba = new QByteArray((char*)image.data, image.total() * image.elemSize());
        Q_EMIT roiReady(ba);
    }
}

void HddlChannel::sendRoiData(QByteArray* ba)
{
    m_client->sendByteArray(ba);
    delete ba;
}

gboolean HddlChannel::busCallBack(GstBus* bus, GstMessage* msg, gpointer data)
{
    HddlChannel* obj = (HddlChannel*)data;
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError* err = NULL;
        gchar* dbg = NULL;
        gst_message_parse_error(msg, &err, &dbg);
        if (err) {
            qDebug() << "ERROR: " << err->message;
            g_error_free(err);
        }
        if (dbg) {
            qDebug() << "Debug details: " << dbg;
            g_free(dbg);
        }
        break;
    }

    case GST_MESSAGE_EOS:
        qDebug() << "EOS";
        obj->m_probPad->reset();
        break;
    default:
        break;
    }

    return true;
}

GstFlowReturn HddlChannel::new_sample(GstElement* sink, gpointer data)
{
    HddlChannel* obj = (HddlChannel*)data;
    GstSample* sample;

    /* Retrieve the buffer */
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (sample) {
        /* The only thing we do in this example is print a * to indicate a received buffer */
        GstBuffer* metaBuffer = gst_sample_get_buffer(sample);
        if (metaBuffer) {
            GstClockTime pts = GST_BUFFER_PTS(metaBuffer);

            GstVideoRegionOfInterestMeta* meta_orig = NULL;
            gpointer state = NULL;
            gboolean needSend = FALSE;
            while ((meta_orig = (GstVideoRegionOfInterestMeta*)
                        gst_buffer_iterate_meta_filtered(metaBuffer,
                            &state,
                            gst_video_region_of_interest_meta_api_get_type()))) {
                std::string label = "null";
                if (g_quark_to_string(meta_orig->roi_type)) {
                    label = g_quark_to_string(meta_orig->roi_type);
                }
                obj->m_sender.serializeSave(meta_orig->x, meta_orig->y, meta_orig->w, meta_orig->h, label, GST_BUFFER_PTS(metaBuffer), 1.0);
                needSend = TRUE;
            }
            if (needSend) {
                obj->m_sender.send();
            }
        }
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }
    return GST_FLOW_ERROR;
}
