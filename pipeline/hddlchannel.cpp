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

HddlChannel::HddlChannel(int channelId, QWidget* parent)
    : QMainWindow(parent)
    , m_id(channelId)
    , m_pipeline(std::make_shared<Pipeline>())
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
    m_stop.store(true);
    if (m_roiThread.joinable()) {
        m_roiThread.join();
    }
}

bool HddlChannel::setupPipeline(const QString& pipelineDescription, const QString& displaySinkName)
{
    if (m_pipeline->parse(pipelineDescription.toStdString().c_str(), displaySinkName.toStdString().c_str())) {
        startTimer(1000);
    }
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
    WId xwinid = this->centralWidget()->winId();
    if (!m_pipeline->run(xwinid)) {
        QTimer::singleShot(0, QApplication::activeWindow(), SLOT(quit()));
    }
}

void HddlChannel::resizeEvent(QResizeEvent* event)
{
    m_pipeline->exposeVideoOverlay();
    QMainWindow::resizeEvent(event);
}

void HddlChannel::timerEvent(QTimerEvent* event)
{
    auto fps = m_pipeline->getFps();
    m_dispatcher->sendString(QString::number(fps, 'f', 2));
}

void HddlChannel::processAction(PipelineAction action)
{
    m_pipeline->process(action);
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
