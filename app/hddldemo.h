#ifndef HDDLDEMO_H
#define HDDLDEMO_H

#include <QMainWindow>
#include <QMap>
#include <QWindow>

class SocketServer;
class QLabel;

namespace Ui {
class HDDLDemo;
}

class HDDLDemo : public QMainWindow {
    Q_OBJECT

public:
    explicit HDDLDemo(QWidget* parent = 0);
    ~HDDLDemo();

public Q_SLOTS:
    void channelWIDReceived(qintptr, WId);
    void channelFpsReceived(qintptr, QString);
    void channelRoiReceived(qintptr, QByteArray*);
    void updateTotalFps();

private:
    void runPipeline();
    void initConfig();

    int m_rows { 0 }; // channel rows
    int m_cols { 0 }; // channel cols
    int m_launchedNum { 0 }; // nums of launched hddlpipelines
    int m_embededNum { 0 }; // nums of launched hddlpipelines which are embeded in GUI
    std::vector<std::string> m_pipeline {}; // channel gstreamer pipeline
    std::vector<std::string> m_mediaFiles {}; // channel media input files
    QString m_classificationModelPath;
    QString m_detectionModelPath;

    QTimer* m_pipelineTimer; // channel launch timer
    QTimer* m_totalFpsTimer; // total fps update timer

    QMap<qintptr, qint32> m_socketToIndex;
    QMap<qint32, int> m_channelToRoiNum;

    SocketServer* m_server;
    Ui::HDDLDemo* ui;
};

#endif // HDDLDEMO_H
