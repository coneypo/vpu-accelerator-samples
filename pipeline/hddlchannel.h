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

#include "utils/messagetype.h"

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

public Q_SLOTS:
    void processAction(PipelineAction action);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    void fetchRoiData();
    void sendRoiData(QByteArray* ba);
    static gboolean busCallBack(GstBus* bus, GstMessage* msg, gpointer data);

    int m_id;
    GstElement* m_pipeline { nullptr };
    GstElement* m_displaySink { nullptr };
    GstVideoOverlay* m_overlay { nullptr };
    FpsStat* m_probPad { nullptr };
    AppConnector* m_client { nullptr };

    std::thread m_roiThread;
    std::atomic<bool> m_stop { false };
};

#endif // HDDLPIPELINE_H
