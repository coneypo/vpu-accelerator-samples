#include "hddldemo.h"
#include "cropdefs.h"
#include "messagetype.h"
#include "socketserver.h"
#include "ui_hddldemo.h"

#include <QLabel>
#include <QProcess>
#include <QPixmap>


HDDLDemo::HDDLDemo(QString pipeline, int rows, int cols, QWidget *parent) :
    QMainWindow(parent),
    m_rows(rows),
    m_cols(cols),
    m_launchedNum(0),
    m_embededNum(0),
    m_pipeline(pipeline),
    ui(new Ui::HDDLDemo)
{
    ui->setupUi(this);
    m_pipelineTimer = new QTimer(this);
    m_totalFpsTimer = new QTimer(this);

    for(int i = 0; i < m_rows; i++){
        ui->grid_layout->setRowStretch(i,1);
        for(int j = 0;j < m_cols; j++){
            m_channelToRoiNum[i*m_cols+j] = 0;
            if( i == 0){
                ui->grid_layout->setColumnStretch(j, 1);
            }
            //create container
            QFrame* frame = new QFrame(this);
            QString frameName = QString("frame_") + QString::number(i*m_cols + j);
            frame->setObjectName(frameName);
            frame->setStyleSheet(QString("#%1 { border: 1px solid white; }").arg(frameName));
            ui->grid_layout->addWidget(frame,i,j);

            //create video stream layout
            QVBoxLayout* layout  = new QVBoxLayout(frame);

            QHBoxLayout* hlayout  = new QHBoxLayout();
            hlayout->setAlignment(Qt::AlignTop);
            //create video stream label
            QLabel * fpstxt_label = new QLabel(frame);
            fpstxt_label->setObjectName(QString("label_stream_") + QString::number(i*m_cols + j));
            fpstxt_label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            fpstxt_label->setText(QString("CH")+QString::number(i*m_cols + j)+QString(" FPS:"));
            fpstxt_label->setStyleSheet("QLabel {color : white;}");
            hlayout->addWidget(fpstxt_label);

            QLabel * fps_label = new QLabel(frame);
            fps_label->setObjectName(QString("label_fpsstream_") + QString::number(i*m_cols + j));
            fps_label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            fps_label->setText(QString("-N-A-"));
            fps_label->setStyleSheet("QLabel {color : white;}");
            hlayout->addWidget(fps_label);


            QSpacerItem* hspacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
            hlayout->addItem(hspacer);
            hlayout->setContentsMargins(0, 0, 0, 0);


            hlayout->setContentsMargins(1, 1, 0, 0);
            hlayout->setSizeConstraint(QLayout::SetFixedSize);


            layout->addLayout(hlayout);
            layout->setContentsMargins(1, 1, 1, 1);

            frame->setLayout(layout);


#if 1
            //add result display
            QFrame* resultContainer = ui->centralWidget->findChild<QFrame*>("frame_display");
            if(!resultContainer){
                qCritical()<<"crop display widget not found!";
            }
            QVBoxLayout* display_layout  = new QVBoxLayout(resultContainer);
            if(!resultContainer->layout()){
                resultContainer->setLayout(display_layout);
            }

            QFrame* result_frame = new QFrame(resultContainer);
            QString result_frame_name = QString("result_frame_")+QString::number(i*m_cols + j);
            result_frame->setObjectName(result_frame_name);
            QHBoxLayout* resultLayout = new QHBoxLayout(result_frame);
            result_frame->setStyleSheet(QString("#%1 { border: 1px solid white; }  QLabel {color : white; }").arg(result_frame_name));
            result_frame->setLayout(resultLayout);
            QLabel* label_channel = new QLabel(resultContainer);
            label_channel->setText(QString("CH %1").arg(i*m_cols +j ));
            resultLayout->addWidget(label_channel);
            for(int index = 0; index< CROP_ROI_NUM; index++){
                QLabel* label_roi = new QLabel(resultContainer);
                label_roi->setObjectName(QString("result_roi_")+QString::number(i*m_cols + j) + QString("_") + QString::number(index));
                resultLayout->addWidget(label_roi);
            }
            resultLayout->addStretch();
            resultContainer->layout()->addWidget(result_frame);
            this->adjustSize();
#endif
        }
    }

    m_server = new SocketServer("hddldemo", this);
    connect(m_server, &SocketServer::WIDReceived, this, &HDDLDemo::channelWIDReceived);
    connect(m_server, &SocketServer::StringReceived, this,&HDDLDemo::channelFpsReceived);
    connect(m_server, &SocketServer::ByteArrayReceived, this,&HDDLDemo::channelRoiReceived);
    connect(m_pipelineTimer,&QTimer::timeout, this, &HDDLDemo::runPipeline);
    connect(m_totalFpsTimer,&QTimer::timeout, this, &HDDLDemo::updateTotalFps);

    m_pipelineTimer->start(500);
    m_totalFpsTimer->start();
}

HDDLDemo::~HDDLDemo()
{
    delete ui;
}



void HDDLDemo::channelWIDReceived(qintptr sd, WId wid){
    QWindow *container = QWindow::fromWinId(wid);
    QString name = QString("Pipeline_%1").arg(m_launchedNum);
    QWidget *stream= QWidget::createWindowContainer(container,centralWidget());
    stream->setObjectName(name);
    stream->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QFrame * frame_container = this->findChild<QFrame*>(QString("frame_%1").arg(m_embededNum));
    if(frame_container){
        frame_container->layout()->addWidget(stream);
        m_socketToIndex.insert(sd, m_embededNum);
        m_embededNum++;
    }
}

void HDDLDemo::channelFpsReceived(qintptr sp, QString text){
    QLabel* fps_label = this->findChild<QLabel*>(QString("label_fpsstream_%1").arg(m_socketToIndex[sp]));
    if(fps_label){
        fps_label->setText(text);
    }
}

void HDDLDemo::channelRoiReceived(qintptr sp, QByteArray *ba){
    int index = m_channelToRoiNum[m_socketToIndex[sp]];
    m_channelToRoiNum[m_socketToIndex[sp]] = (index + 1) % CROP_ROI_NUM;
    QString name = QString("result_roi_%1_%2").arg(m_socketToIndex[sp]).arg(index) ;
    QLabel* label_roi = this->findChild<QLabel*>(name);
    if(label_roi){
        label_roi->setPixmap(QPixmap::fromImage(QImage((uchar*)ba->data(),CROP_IMAGE_WIDTH, CROP_IMAGE_HEIGHT, QImage::Format_RGB888)));
    }
    delete ba;
}

void HDDLDemo::runPipeline(){
    if(m_launchedNum < m_cols * m_rows){
        QProcess* gstreamer_process = new QProcess(this);
        gstreamer_process->setProcessChannelMode(QProcess::ForwardedChannels);
        QString gstreamer_cmd = QString("./hddlpipeline");
        QString ipc_name = "/var/tmp/gstreamer_ipc_" + QString::number(m_launchedNum);
        auto arguments = m_pipeline.arg(ipc_name).split(" ");
        arguments.append(QString::number(m_launchedNum));
        gstreamer_process->start(gstreamer_cmd, arguments);

        m_launchedNum++;
    }else{
        m_pipelineTimer->stop();
    }
}

void HDDLDemo::updateTotalFps(){
    double total = 0;
    for(int i=0; i< m_embededNum; i++){
        QLabel* fps_label = this->findChild<QLabel*>(QString("label_fpsstream_%1").arg(i));
        if(fps_label){
            QString fps = fps_label->text();
            total +=  fps.toDouble();
        }
    }
    ui->label_decode_fps->setText(QString::number(total,'f',2));
}

