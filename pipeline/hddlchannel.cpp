#include "hddlchannel.h"
#include "blockingqueue.h"
#include "cropdefs.h"
#include "fpsstat.h"
#include "messagedispatcher.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QTimer>
#include <QWidget>

#include <memory>
#include <opencv2/opencv.hpp>

#define OVERLAY_NAME "mfxsink0"

HddlChannel::HddlChannel(int channelId, QWidget* parent)
    : QMainWindow(parent)
    , m_id(channelId)
{
    QWidget* container = new QWidget(this);
    setCentralWidget(container);
    container->setStyleSheet("background-color: rgb(50, 50, 78);");
    this->resize(QApplication::primaryScreen()->size());

    connect(this, &HddlChannel::roiReady, this, &HddlChannel::sendRoiData);

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
    if (m_roiThread.joinable()) {
        m_roiThread.join();
    }
}

bool HddlChannel::setupPipeline(const QString& pipelineDescription, const QString& displaySinkName)
{
    // init gstreamer
    gst_init(NULL, NULL);

    //parse gstreamer pipeline
    m_pipeline = gst_parse_launch_full(pipelineDescription.toStdString().c_str(), NULL, GST_PARSE_FLAG_FATAL_ERRORS, NULL);
    GstElement* dspSink = gst_bin_get_by_name(GST_BIN(m_pipeline), displaySinkName.toStdString().c_str());
    Q_ASSERT(dspSink != nullptr);

    //get gstreamer displaysink video overlay
    m_overlay = GST_VIDEO_OVERLAY(gst_bin_get_by_name(GST_BIN(dspSink), OVERLAY_NAME));
    //Q_ASSERT(m_overlay != nullptr);

    GstBus* bus = gst_element_get_bus(m_pipeline);
    gst_bus_add_watch(bus, HddlChannel::busCallBack, this);
    gst_object_unref(bus);

    //probe sinkpad for calculate fps
    GstPad* sinkPad = gst_element_get_static_pad(dspSink, "sink");
    Q_ASSERT(sinkPad != nullptr);
    m_probPad = new FpsStat(sinkPad);
    startTimer(1000);

    return true;
}

bool HddlChannel::initConnection(const QString& serverPath)
{
    m_dispatcher = new MessageDispatcher(serverPath, this);
    if (!m_dispatcher->connectServer()) {
        return false;
    };
    m_dispatcher->sendWinId(this->winId());
    connect(m_dispatcher, &MessageDispatcher::actionReceived, this, &HddlChannel::processAction);
    return true;
}

void HddlChannel::run()
{
    if (m_overlay) {
        WId xwinid = this->centralWidget()->winId();
        gst_video_overlay_set_window_handle(m_overlay, xwinid);
    }

    auto ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        QTimer::singleShot(0, QApplication::activeWindow(), SLOT(quit()));
    }
}

void HddlChannel::resizeEvent(QResizeEvent* event)
{
    if (m_overlay) {
        gst_video_overlay_expose(m_overlay);
    }
    QMainWindow::resizeEvent(event);
}

void HddlChannel::timerEvent(QTimerEvent* event)
{
    auto fps = m_probPad->getRenderFps();
    m_dispatcher->sendString(QString::number(fps, 'f', 2));
}

void HddlChannel::processAction(PipelineAction action)
{
    QString actionStr;
    switch (action) {
    case MESSAGE_START:
        gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        break;
    case MESSAGE_STOP:
        gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
        break;

    case MESSAGE_REPLAY: {
        gst_pad_push_event(gst_element_get_static_pad(m_displaySink, "src"), gst_event_new_eos());
        gst_element_seek_simple(m_pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_KEY_UNIT, 0);
        gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        break;
    }
    default:
        qDebug() << "Unrecognized action";
    }
}

void HddlChannel::fetchRoiData()
{
    while (!m_stop) {
        auto src = BlockingQueue<std::shared_ptr<cv::UMat>>::instance().take();
        cv::Mat image;
        cv::resize(*src, image, cv::Size(CROP_IMAGE_WIDTH, CROP_IMAGE_HEIGHT));
        QByteArray* ba = new QByteArray((char*)image.data, image.total() * image.elemSize());
        Q_EMIT roiReady(ba);
    }
}

void HddlChannel::sendRoiData(QByteArray* ba)
{
    m_dispatcher->sendByteArray(ba);
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
