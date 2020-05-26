#include "hddlchannel.h"
#include "blockingqueue.h"
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
}

HddlChannel::~HddlChannel()
{
    m_pipeline.reset();
    qDebug() << "Hddl Pipeline" << m_id << "is closed";
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

void HddlChannel::run(int timeout)
{
    WId xwinid = this->centralWidget()->winId();
    if (!m_pipeline->run(xwinid)) {
        QTimer::singleShot(0, QApplication::activeWindow(), SLOT(quit()));
    }

    if (timeout > 0) {
        QTimer::singleShot(std::chrono::seconds(timeout), this, [this]() {
            QApplication::quit();
        });
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
    auto offloadInferFps = m_pipeline->getOffloadPipeInferenceFps();
    auto offloadDecFps = m_pipeline->getOffloadPipeDecodingFps();
    m_dispatcher->sendString(QString::number(fps, 'f', 2) + ":" + QString::number(offloadInferFps, 'f', 2) + ":" + QString::number(offloadDecFps, 'f', 2));
    fetchRoiData();
}

void HddlChannel::keyPressEvent(QKeyEvent* event)
{
    if ((event->key() == Qt::Key_Q) || (event->key() == Qt::Key_Escape)) {
        m_dispatcher->sendStop();
    }
}

void HddlChannel::processAction(PipelineAction action)
{
    m_pipeline->process(action);
    if (action == MESSAGE_STOP) {
        QApplication::quit();
    }
}

void HddlChannel::fetchRoiData()
{

    std::shared_ptr<cv::Mat> src;
    if (BlockingQueue<std::shared_ptr<cv::Mat>>::instance().tryTake(src, 100)) {
        QByteArray* ba = new QByteArray((char*)src->data, src->total() * src->elemSize());
        Q_EMIT roiReady(ba);
    }
}

void HddlChannel::sendRoiData(QByteArray* ba)
{
    m_dispatcher->sendByteArray(ba);
    delete ba;
}
