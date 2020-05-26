#ifndef HDDLDEMO_H
#define HDDLDEMO_H

#include <QMainWindow>
#include <QMap>
#include <QWindow>
#include <utils/messagetype.h>

class Dispatcher;
class QLabel;
class QProcess;

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
    void channelActionReceived(qintptr, PipelineAction action);
    void updateTotalFps();

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;

private:
    void runPipeline();
    void initConfig();
    void setTextOnLabel(const QString& labelName, const QString& text);
    void setupHvaProcess();
    void sendSignalToHvaPipeline();

    uint32_t m_rows { 0 }; // channel rows
    uint32_t m_cols { 0 }; // channel cols
    uint32_t m_launchedNum { 0 }; // nums of launched hddlpipelines
    uint32_t m_embededNum { 0 }; // nums of launched hddlpipelines which are embeded in GUI
    uint32_t m_timeout { 0 };
    std::vector<std::string> m_pipeline {}; // channel gstreamer pipeline

    QTimer* m_pipelineTimer; // channel launch timer
    QTimer* m_totalFpsTimer; // total fps update timer
    std::vector<QProcess*> m_pipelineProcesses;
    QProcess* m_hvaProcess { nullptr };

    QMap<qintptr, qint32> m_socketToIndex;
    QMap<qint32, int> m_channelToRoiNum;

    Dispatcher* m_dispatcher;
    Ui::HDDLDemo* ui;
};

#endif // HDDLDEMO_H
