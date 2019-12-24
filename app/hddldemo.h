#ifndef HDDLDEMO_H
#define HDDLDEMO_H

#include <QMainWindow>
#include <QWindow>
#include <QMap>

class SocketServer;
class QLabel;

namespace Ui {
class HDDLDemo;
}

class HDDLDemo : public QMainWindow
{
    Q_OBJECT

public:
    explicit HDDLDemo(QString pipeline, int rows, int cols, QWidget *parent = 0);
    ~HDDLDemo();

public Q_SLOTS:
    void channelWIDReceived(qintptr, WId);
    void channelFpsReceived(qintptr, QString);
    void channelRoiReceived(qintptr, QByteArray*);
    void updateTotalFps();

private:
    void runPipeline();

    int m_rows;   			// channel rows
    int m_cols;  			// channel cols
    int m_launchedNum;		// nums of launched hddlpipelines
    int m_embededNum;		// nums of launched hddlpipelines which are embeded in GUI
    QString m_pipeline;		// channel gstreamer pipeline

    QTimer *m_pipelineTimer;   // channel launch timer
    QTimer *m_totalFpsTimer;   // total fps update timer

    QMap<qintptr, qint32> m_socketToIndex;
    QMap<qint32, int> m_channelToRoiNum;

    SocketServer* m_server;
    Ui::HDDLDemo *ui;
};

#endif // HDDLDEMO_H
