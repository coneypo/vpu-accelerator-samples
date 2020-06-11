#include "hddldemo.h"
#include "configparser.h"
#include "cropdefs.h"
#include "dispatcher.h"
#include "ui_hddldemo.h"
#include "utils/ipc.h"

#include <QLabel>
#include <QPixmap>
#include <QProcess>
#include <QDesktopWidget>
#include <chrono>
#include <csignal>
#include <thread>

HDDLDemo::HDDLDemo(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::HDDLDemo)
{
    ui->setupUi(this);
    m_pipelineTimer = new QTimer(this);
    m_totalFpsTimer = new QTimer(this);

    initConfig();

    for (uint32_t i = 0; i < m_rows; i++) {
        ui->grid_layout->setRowStretch(i, 1);
        for (uint32_t j = 0; j < m_cols; j++) {
            m_channelToRoiNum[i * m_cols + j] = 0;
            if (i == 0) {
                ui->grid_layout->setColumnStretch(j, 1);
            }
            //create container
            QFrame* frame = new QFrame(this);
            QString frameName = QString("frame_") + QString::number(i * m_cols + j);
            frame->setObjectName(frameName);
            frame->setStyleSheet(QString("#%1 { border: 1px solid white; }").arg(frameName));
            ui->grid_layout->addWidget(frame, i, j);

            //create video stream layout
            QVBoxLayout* layout = new QVBoxLayout(frame);

            QHBoxLayout* hlayout = new QHBoxLayout();
            hlayout->setAlignment(Qt::AlignTop);
            //create video stream label
            QLabel* fpstxt_label = new QLabel(frame);
            fpstxt_label->setObjectName(QString("label_stream_") + QString::number(i * m_cols + j));
            fpstxt_label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            fpstxt_label->setText(QString("CH") + QString::number(i * m_cols + j) + QString(" FPS:"));
            fpstxt_label->setStyleSheet("QLabel {color : white;}");
            hlayout->addWidget(fpstxt_label);

            QLabel* fps_label = new QLabel(frame);
            fps_label->setObjectName(QString("label_fpsstream_") + QString::number(i * m_cols + j));
            fps_label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            fps_label->setText(QString("-N-A-"));
            fps_label->setStyleSheet("QLabel {color : white;}");
            hlayout->addWidget(fps_label);

            QLabel* infer_fps_label = new QLabel(frame);
            infer_fps_label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            infer_fps_label->setText(QString("Infer FPS:"));
            infer_fps_label->setStyleSheet("QLabel {color : white;}");
            hlayout->addWidget(infer_fps_label);

            QLabel* infer_fps_value_label = new QLabel(frame);
            infer_fps_value_label->setObjectName(QString("label_inferfpsstream_") + QString::number(i * m_cols + j));
            infer_fps_value_label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            infer_fps_value_label->setText(QString("-N-A-"));
            infer_fps_value_label->setStyleSheet("QLabel {color : white;}");
            hlayout->addWidget(infer_fps_value_label);

            QLabel* offload_dec_fps_label = new QLabel(frame);
            offload_dec_fps_label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            offload_dec_fps_label->setText(QString("Decoding FPS:"));
            offload_dec_fps_label->setStyleSheet("QLabel {color : white;}");
            hlayout->addWidget(offload_dec_fps_label);

            QLabel* offload_dec_value_label = new QLabel(frame);
            offload_dec_value_label->setObjectName(QString("label_decfpsstream_") + QString::number(i * m_cols + j));
            offload_dec_value_label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            offload_dec_value_label->setText(QString("-N-A-"));
            offload_dec_value_label->setStyleSheet("QLabel {color : white;}");
            hlayout->addWidget(offload_dec_value_label);

            QSpacerItem* hspacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
            hlayout->addItem(hspacer);
            hlayout->setContentsMargins(1, 1, 0, 0);
            hlayout->setSizeConstraint(QLayout::SetFixedSize);

            layout->addLayout(hlayout);
            layout->setContentsMargins(1, 1, 1, 1);
            frame->setLayout(layout);
            //add result display
            QFrame* resultContainer = ui->centralWidget->findChild<QFrame*>("frame_display");
            if (!resultContainer) {
                qCritical() << "crop display widget not found!";
            } else {
                QVBoxLayout* display_layout = new QVBoxLayout(resultContainer);
                if (!resultContainer->layout()) {
                    resultContainer->setLayout(display_layout);
                }

                QFrame* result_frame = new QFrame(resultContainer);
                QString result_frame_name = QString("result_frame_") + QString::number(i * m_cols + j);
                result_frame->setObjectName(result_frame_name);
                QHBoxLayout* resultLayout = new QHBoxLayout(result_frame);
                result_frame->setStyleSheet(QString("#%1 { border: 1px solid white; }  QLabel {color : white; }").arg(result_frame_name));
                result_frame->setLayout(resultLayout);
                QLabel* label_channel = new QLabel(resultContainer);
                label_channel->setText(QString("CH %1").arg(i * m_cols + j));
                resultLayout->addWidget(label_channel);
                for (int index = 0; index < CROP_ROI_NUM; index++) {
                    QLabel* label_roi = new QLabel(resultContainer);
                    label_roi->setObjectName(QString("result_roi_") + QString::number(i * m_cols + j) + QString("_") + QString::number(index));
                    resultLayout->addWidget(label_roi);
                }
                resultLayout->addStretch();
                resultContainer->layout()->addWidget(result_frame);
                this->adjustSize();
            }
        }
    }

    m_dispatcher = new Dispatcher("hddldemo", this);
    connect(m_dispatcher, &Dispatcher::WIDReceived, this, &HDDLDemo::channelWIDReceived);
    connect(m_dispatcher, &Dispatcher::StringReceived, this, &HDDLDemo::channelFpsReceived);
    connect(m_dispatcher, &Dispatcher::ByteArrayReceived, this, &HDDLDemo::channelRoiReceived);
    connect(m_dispatcher, &Dispatcher::ActionReceived, this, &HDDLDemo::channelActionReceived);
    connect(m_pipelineTimer, &QTimer::timeout, this, &HDDLDemo::runPipeline);
    connect(m_totalFpsTimer, &QTimer::timeout, this, &HDDLDemo::updateTotalFps);

    m_pipelineTimer->start(500);
    m_totalFpsTimer->start();
}

HDDLDemo::~HDDLDemo()
{
    m_dispatcher->sendAction(MESSAGE_STOP);
    for (auto& process : m_pipelineProcesses) {
        if (!process->waitForFinished()) {
            process->kill();
        }
        delete process;
    }
    m_pipelineProcesses.clear();

    if (m_hvaProcess) {
        qDebug() << "Try to send SIGINT to process:" << m_hvaProcess->pid();
        if (!kill(m_hvaProcess->pid(), SIGINT)) {
            qDebug() << "Send SIGINT done";
        } else {
            qDebug() << "Send SIGINT failed";
        }
        if (m_hvaProcess->waitForFinished()) {
            delete m_hvaProcess;
        }
    }

    delete ui;
    qDebug() << "Hddl Demo is closed";
}

void HDDLDemo::channelWIDReceived(qintptr sd, WId wid)
{
    QWindow* container = QWindow::fromWinId(wid);
    QString name = QString("Pipeline_%1").arg(m_launchedNum);
    QWidget* stream = QWidget::createWindowContainer(container, centralWidget());
    stream->setObjectName(name);
    stream->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QFrame* frame_container = this->findChild<QFrame*>(QString("frame_%1").arg(m_embededNum));
    frame_container->setFixedSize(frame_container->size());
    if (frame_container) {
        frame_container->layout()->addWidget(stream);
        m_socketToIndex.insert(sd, m_embededNum);
        m_embededNum++;
    }
}

void HDDLDemo::setTextOnLabel(const QString& labelName, const QString& text)
{
    QLabel* label = this->findChild<QLabel*>(labelName);
    if (label) {
        label->setText(text);
    }
}

void HDDLDemo::channelFpsReceived(qintptr sp, QString text)
{
    auto fpsList = text.split(":");
    if (fpsList.size() == 3) {
        setTextOnLabel(QString("label_fpsstream_%1").arg(m_socketToIndex[sp]), fpsList[0]);
        setTextOnLabel(QString("label_inferfpsstream_%1").arg(m_socketToIndex[sp]), fpsList[1]);
        setTextOnLabel(QString("label_decfpsstream_%1").arg(m_socketToIndex[sp]), fpsList[2]);
    }
}

void HDDLDemo::channelRoiReceived(qintptr sp, QByteArray* ba)
{
    QFrame* resultContainer = ui->centralWidget->findChild<QFrame*>("frame_display");
    QFrame* frameStatistic= ui->centralWidget->findChild<QFrame*>("frame_statistic");
    static int frameWidth = resultContainer->width() / 4;
    static int frameHeight = (QApplication::desktop()->screenGeometry().height() - frameStatistic->height())/m_rows/m_cols * 0.4 ;
    int size = std::min(frameHeight, frameWidth);

    int index = m_channelToRoiNum[m_socketToIndex[sp]];
    m_channelToRoiNum[m_socketToIndex[sp]] = (index + 1) % CROP_ROI_NUM;
    QString name = QString("result_roi_%1_%2").arg(m_socketToIndex[sp]).arg(index);
    QLabel* label_roi = this->findChild<QLabel*>(name);
    if (label_roi) {
        label_roi->setPixmap(QPixmap::fromImage(QImage((uchar*)ba->data(), CROP_IMAGE_WIDTH, CROP_IMAGE_HEIGHT, QImage::Format_RGB888)).scaled(size, size));
    }
    delete ba;
}

void HDDLDemo::channelActionReceived(qintptr, PipelineAction action)
{
    Q_UNUSED(action);
    m_dispatcher->sendAction(MESSAGE_STOP);
    QApplication::quit();
}

void HDDLDemo::initConfig()
{
    m_pipeline = ConfigParser::instance()->getPipelines();
    m_timeout = ConfigParser::instance()->getTimeout();
    m_cols = std::ceil(std::sqrt(m_pipeline.size()));
    m_rows = std::ceil(m_pipeline.size() * 1.0 / m_cols);
    //launch hva process and send channel socket address to it
    if (ConfigParser::instance()->isHvaConfigured()) {
        setupHvaProcess();
        QThread::sleep(2);
        sendSignalToHvaPipeline();
    }
}

void HDDLDemo::runPipeline()
{
    if (m_launchedNum < m_pipeline.size()) {
        QString hddlChannelCmd = QString("./hddlpipeline");
        QProcess* hddlChannelProcess = new QProcess(this);
        hddlChannelProcess->setProcessChannelMode(QProcess::ForwardedChannels);
        QStringList arguments;
        arguments.push_back("--pipeline");
        arguments.push_back(QString::fromStdString(m_pipeline[m_launchedNum]));
        arguments.push_back("--index");
        arguments.push_back(QString::number(m_launchedNum));
        arguments.push_back("--timeout");
        arguments.append(QString::number(m_timeout));
        hddlChannelProcess->start(hddlChannelCmd, arguments);
        m_pipelineProcesses.push_back(hddlChannelProcess);
        m_launchedNum++;
    } else {
        m_pipelineTimer->stop();
    }
}

void HDDLDemo::updateTotalFps()
{
    double pipelineTotal = 0;
    double inferTotal = 0;
    for (uint32_t i = 0; i < m_embededNum; i++) {
        QLabel* fps_label = this->findChild<QLabel*>(QString("label_fpsstream_%1").arg(i));
        if (fps_label) {
            QString fps = fps_label->text();
            pipelineTotal += fps.toDouble();
        }

        QLabel* inferfps_label = this->findChild<QLabel*>(QString("label_inferfpsstream_%1").arg(i));
        if (inferfps_label) {
            QString fps = inferfps_label->text();
            inferTotal += fps.toDouble();
        }
    }
    ui->label_decode_fps->setText(QString::number(pipelineTotal, 'f', 2));
    ui->label_inference_fps->setText(QString::number(inferTotal, 'f', 2));
}

void HDDLDemo::setupHvaProcess()
{
    QString hvaCmd = QString::fromStdString(ConfigParser::instance()->getHvaCMd());
    QString hvaWorkDirectory = QString::fromStdString(ConfigParser::instance()->getHvaWorkDirectory());
    auto hvaEnvironmentVariables = ConfigParser::instance()->getHvaEnvironmentVariables();
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (auto& entry : hvaEnvironmentVariables) {
        env.insert(QString::fromStdString(entry.first), QString::fromStdString(entry.second));
    }
    m_hvaProcess = new QProcess();
    m_hvaProcess->setProcessEnvironment(env);
    m_hvaProcess->setWorkingDirectory(hvaWorkDirectory);
    m_hvaProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    m_hvaProcess->start(hvaCmd);
    m_hvaProcess->waitForStarted();
}

void HDDLDemo::sendSignalToHvaPipeline()
{
    auto hvaServerAddr = ConfigParser::instance()->getHvaSocketPath();

    auto poller = HddlUnite::Poller::create("hva_client");
    auto conn = HddlUnite::Connection::create(poller);

    while (!conn->connect(hvaServerAddr)) {
        qDebug() << "try to connect " << QString::fromStdString(hvaServerAddr);
        QThread::sleep(1);
    }

    std::lock_guard<std::mutex> lock(conn->getMutex());

    // send channel number to hva
    //int channelSize = static_cast<int>(m_pipeline.size());
    //if (conn->write(&channelSize, sizeof(channelSize)) != sizeof(channelSize)) {
    //    qDebug() << "Failed to send channel number!";
    //    exit(EXIT_FAILURE);
    //}

    std::string msgToSend = "";
    for (auto& pipeParam : ConfigParser::instance()->getPipelineParams()) {
        auto channelSocket = pipeParam["socket_name"];
        msgToSend = msgToSend + "," + channelSocket;
    }
    if (msgToSend.empty()) {
        qDebug() << "Failed to get channel socket!";
        exit(EXIT_FAILURE);
    }
    msgToSend.erase(0, 1);

    int length = static_cast<int>(msgToSend.length());

    //send socket length to hva
    int result = conn->write((void*)&length, sizeof(length));
    if (result != sizeof(length)) {
        qDebug() << "Failed to send socket length!";
        exit(EXIT_FAILURE);
    }
    qDebug() << "Sent length " << length << " in " << sizeof(length) << " bytes";

    //send socket name to hva
    result = conn->write(&msgToSend[0], length);
    if (result != length) {
        qDebug() << "Failed to send socket addr!";
        exit(EXIT_FAILURE);
    }
    qDebug() << "Sent msg " << QString::fromStdString(msgToSend) << " in " << length << " bytes";
}

void HDDLDemo::keyPressEvent(QKeyEvent* event)
{
    if ((event->key() == Qt::Key_Q) || (event->key() == Qt::Key_Escape)) {
        m_dispatcher->sendAction(MESSAGE_STOP);
        QApplication::quit();
    }
}
