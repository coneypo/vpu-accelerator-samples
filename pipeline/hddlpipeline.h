#ifndef HDDLPIPELINE_H
#define HDDLPIPELINE_H

#include <QMainWindow>
#include <QImage>
#include <thread>
#include <atomic>
#include <chrono>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

using namespace std::chrono;

class SocketClient;
class FpsStat;
class QTimer;

class HddlPipeline : public QMainWindow
{
    Q_OBJECT
public:
    HddlPipeline(QString pipeline, QString displaySinkName, QWidget *parent = 0);
    ~HddlPipeline();
    void run();

Q_SIGNALS:
    void roiReady(QByteArray* ba);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void sendFps();
    void fetchRoiData();
    void sendRoiData(QByteArray* ba);

    static gboolean busCallBack(GstBus *bus, GstMessage *msg, gpointer data);

    QString m_config;
    GstElement *m_pipeline;
    GstVideoOverlay* m_overlay;
    SocketClient *m_client;
    FpsStat* m_probPad;
    QTimer* m_fpstimer;

    std::thread m_roiThread;
    std::atomic<bool> m_stop;
};

#endif // HDDLPIPELINE_H
