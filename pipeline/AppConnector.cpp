#include "AppConnector.h"
#include "messagetype.h"
#include <QImage>
#include <QLocalSocket>
#include <QThread>
#include <QTimer>
#include <thread>

AppConnector::AppConnector(QString socketName, QObject* parent)
    : QObject(parent)
    , m_socketName(socketName)
{
    m_socket = new QLocalSocket(this);

    connect(m_socket, SIGNAL(connected()), this, SLOT(connectedCallBack()));
    connect(m_socket, SIGNAL(disconnected()), this, SLOT(disconnectedCallBack()));
    connect(m_socket, SIGNAL(readyRead()), this, SLOT(messageReceived()));
}

bool AppConnector::connectServer()
{
    m_socket->connectToServer(m_socketName);
    return m_socket->waitForConnected(10000);
}

void AppConnector::close()
{
    m_stop = true;
    m_socket->abort();
}

void AppConnector::sendByteArray(QByteArray* ba)
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_5);
    out << (quint32)ba->size();
    out << (quint32)MESSAGE_BYTEARRAY;
    out << *ba;
    m_socket->write(block);
    m_socket->flush();
}

void AppConnector::sendImage(QImage* image)
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_5);
    //reserved for msg length
    out << (quint32)0;
    out << (quint32)MESSAGE_IMAGE;
    out << *image;
    out.device()->seek(0);
    out << (quint32)(block.size() - sizeof(quint32));
    m_socket->write(block);
    m_socket->flush();
}

void AppConnector::sendString(QString text)
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_5);
    //reserved for msg length
    out << (quint32)0;
    out << (quint32)MESSAGE_STRING;
    out << text;
    out.device()->seek(0);
    out << (quint32)(block.size() - sizeof(quint32));
    m_socket->write(block);
    m_socket->flush();
}

void AppConnector::sendWinId(WId winid)
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_5);
    //reserved for msg length
    out << (quint32)0;
    out << (quint32)MESSAGE_WINID;
    out << winid;
    out.device()->seek(0);
    out << (quint32)(block.size() - sizeof(quint32));
    m_socket->write(block);
    m_socket->flush();
}

void AppConnector::connectedCallBack()
{
    qDebug() << "socket is connected";
}

void AppConnector::disconnectedCallBack()
{
    qDebug() << "socket is disconnected";
}

void AppConnector::messageReceived()
{
    QDataStream dataStream(m_socket);
    dataStream.setVersion(QDataStream::Qt_5_5);
    while (!dataStream.atEnd()) {
        // action message length should be 12 bytes
        if (m_socket->bytesAvailable() < (int)sizeof(quint32) * 3) {
            return;
        }
        quint32 msgLength;
        dataStream >> msgLength;
        quint32 msgType;
        dataStream >> msgType;
        switch (static_cast<MessageType>(msgType)) {
        case MESSAGE_ACTION: {
            quint32 msgAction;
            dataStream >> msgAction;
            Q_EMIT actionReceived(static_cast<PipelineAction>(msgAction));
            break;
        }
        default:
            qDebug() << "unknown message type, try to skip following content size" << msgLength;
            dataStream.skipRawData(msgLength);
        }
    }
}
