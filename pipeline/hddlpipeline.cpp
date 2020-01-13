#include "hddlpipeline.h"
#include "socketclient.h"
#include "fpsstat.h"
#include "blockingqueue.h"
#include "cropdefs.h"

#include <QApplication>
#include <QWidget>
#include <QImage>
#include <QTimer>
#include <QHBoxLayout>
#include <QDebug>

#include <opencv2/opencv.hpp>
#include <memory>


#define OVERLAY_NAME "mfxsink0"


HddlPipeline::HddlPipeline(QString pipeline, QString displaySinkName, QWidget *parent)
    : QMainWindow(parent),
      m_config(pipeline),
      m_probPad(nullptr),
      m_roiQueue(),
      m_stop(false)
{
    m_client = new SocketClient("hddldemo", this);
    bool connectFlag = m_client->connectServer();

    QWidget* container = new QWidget(this);
    setCentralWidget(container);
    container->setStyleSheet("background-color: rgb(50, 50, 78);");
    this->resize(QApplication::primaryScreen()->size());
    // init gstreamer
    gst_init(NULL,NULL);

    //parse gstreamer pipeline
    m_pipeline = gst_parse_launch_full(m_config.toStdString().c_str(),  NULL, GST_PARSE_FLAG_FATAL_ERRORS, NULL);
    GstElement* dspSink = gst_bin_get_by_name(GST_BIN(m_pipeline), displaySinkName.toStdString().c_str());
    Q_ASSERT(dspSink!=nullptr);

    GstElement* osdparser = gst_bin_get_by_name(GST_BIN(m_pipeline), "osdparser0");
    Q_ASSERT(osdparser!=nullptr);
    g_object_set(osdparser, "roi_queue", &m_roiQueue, NULL);

    //get gstreamer displaysink video overlay
    m_overlay = GST_VIDEO_OVERLAY (gst_bin_get_by_name(GST_BIN (dspSink),OVERLAY_NAME));
    Q_ASSERT(m_overlay!=nullptr);

    GstBus* bus = gst_element_get_bus(m_pipeline);
    gst_bus_add_watch(bus, HddlPipeline::busCallBack, this);
    gst_object_unref(bus);

    if (connectFlag) {
        m_client->sendWinId(this->winId());

        //probe sinkpad for calculate fps
        GstPad* sinkPad = gst_element_get_static_pad(dspSink,"sink");
        Q_ASSERT(sinkPad!=nullptr);
        m_probPad = new FpsStat(sinkPad);

        m_fpstimer = new QTimer(this);
        m_fpstimer->setInterval(1000);
        connect(m_fpstimer, &QTimer::timeout, this, &HddlPipeline::sendFps);
        m_fpstimer->start();

        connect(this, &HddlPipeline::roiReady, this, &HddlPipeline::sendRoiData);
    }
    m_roiThread = std::thread(&HddlPipeline::fetchRoiData,this);
}

HddlPipeline::~HddlPipeline()
{
    if(m_probPad){
        delete m_probPad;
    }

    gst_element_set_state (m_pipeline, GST_STATE_NULL);
    gst_object_unref (m_pipeline);

    m_stop.store(true);
    m_roiThread.join();
}

void  HddlPipeline::run(){
    WId xwinid = this->centralWidget()->winId();
    gst_video_overlay_set_window_handle (m_overlay, xwinid);

    GstStateChangeReturn sret = gst_element_set_state (m_pipeline, GST_STATE_PLAYING);
    if (sret == GST_STATE_CHANGE_FAILURE) {
        QTimer::singleShot(0, QApplication::activeWindow(), SLOT(quit()));
    }
}

void HddlPipeline::resizeEvent(QResizeEvent* event){
    gst_video_overlay_expose(m_overlay);
    QMainWindow::resizeEvent(event);
}


void HddlPipeline::sendFps(){
    auto fps = m_probPad->getRenderFps();
    m_client->sendString(QString::number(fps,'f',2));
}

void HddlPipeline::fetchRoiData(){
    while(!m_stop){
        auto src =  m_roiQueue.take();
        cv::Mat image;
        cv::resize(*src,image,cv::Size(CROP_IMAGE_WIDTH,CROP_IMAGE_HEIGHT));
        QByteArray*  ba = new QByteArray((char*)image.data, image.total()*image.elemSize());
        Q_EMIT roiReady(ba);
    }
}

void HddlPipeline::sendRoiData(QByteArray *ba){
    m_client->sendByteArray(ba);
    delete ba;
}


gboolean HddlPipeline::busCallBack(GstBus *bus, GstMessage *msg, gpointer data){
    HddlPipeline* obj = (HddlPipeline*) data;
    switch(GST_MESSAGE_TYPE(msg)){
    case GST_MESSAGE_ERROR:{
            GError *err = NULL;
            gchar *dbg = NULL;
            gst_message_parse_error(msg, &err, &dbg);
            if (err) {
                qDebug()<<"ERROR: "<<err->message;
                g_error_free(err);
            }
            if (dbg) {
                qDebug()<<"Debug details: "<<dbg;
                g_free(dbg);
            }
            break;
        }

    case GST_MESSAGE_EOS:
        qDebug()<<"EOS";
        obj->m_probPad->reset();
        break;
    default:
        break;
    }

    return true;
}
