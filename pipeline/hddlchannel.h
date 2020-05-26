#ifndef HDDLPIPELINE_H
#define HDDLPIPELINE_H

#include <QImage>
#include <QMainWindow>
#include <atomic>
#include <chrono>
#include <gst/gst.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/videooverlay.h>

#include "pipeline.h"
#include "utils/messagetype.h"

using namespace std::chrono;

class MessageDispatcher;
class FpsStat;
class QTimer;

class HddlChannel : public QMainWindow {
    Q_OBJECT
public:
    explicit HddlChannel(int channelId, QWidget* parent = 0);
    virtual ~HddlChannel();
    bool initConnection(const QString& serverPath = "hddldemo");
    bool setupPipeline(const QString& pipelineDescription, const QString& displaySinkName);
    void run(int timeout = 0);

Q_SIGNALS:
    void roiReady(QByteArray* ba);

public Q_SLOTS:
    void processAction(PipelineAction action);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void fetchRoiData();
    void sendRoiData(QByteArray* ba);

    int m_id;
    MessageDispatcher* m_dispatcher { nullptr };
    std::shared_ptr<Pipeline> m_pipeline { nullptr };
    //std::atomic<bool> m_stop { false };
    //std::thread m_roiThread;
};

#endif // HDDLPIPELINE_H
