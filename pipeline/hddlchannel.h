#ifndef HDDLPIPELINE_H
#define HDDLPIPELINE_H

#include <QImage>
#include <QMainWindow>
#include <atomic>
#include <chrono>
#include <gst/gst.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/videooverlay.h>
#include <opencv2/opencv.hpp>
#include <thread>

#include "blockingqueue.h"
#include "infermetasender.h"

using namespace std::chrono;

class AppConnector;
class FpsStat;
class QTimer;

class HddlChannel : public QMainWindow {
    Q_OBJECT
public:
    explicit HddlChannel(int channelId, QWidget* parent = 0);
    virtual ~HddlChannel();
    bool initConnection(QString serverPath = "hddldemo");
    bool setupPipeline(QString pipelineDescription, QString displaySinkName);
    void run();

Q_SIGNALS:
    void roiReady(QByteArray* ba);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void sendFps();
    void fetchRoiData();
    void sendRoiData(QByteArray* ba);
    static gboolean busCallBack(GstBus* bus, GstMessage* msg, gpointer data);

    int m_id;
    GstElement* m_pipeline;
    GstVideoOverlay* m_overlay;
    AppConnector* m_client;
    FpsStat* m_probPad;
    QTimer* m_fpstimer;
    BlockingQueue<std::shared_ptr<cv::UMat>> m_roiQueue;

    std::thread m_roiThread;
    std::atomic<bool> m_stop;
};

#endif // HDDLPIPELINE_H
