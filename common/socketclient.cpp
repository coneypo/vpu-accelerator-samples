#include "socketclient.h"
#include "messagetype.h"
#include <QLocalSocket>
#include <QTimer>
#include <QImage>

SocketClient::SocketClient(QString socketName, QObject* parent):
    QObject(parent),
    m_socketName(socketName)
{
    m_socket = new QLocalSocket(this);
    connect(m_socket, SIGNAL(connected()),this, SLOT(connectedCallBack()));
    connect(m_socket, SIGNAL(disconnected()),this, SLOT(disconnectedCallBack()));
}

bool SocketClient::connectServer(){
    m_socket->connectToServer(m_socketName);
    return m_socket->waitForConnected(10000);
}

void SocketClient::close(){
    m_socket->abort();
}


void SocketClient::sendByteArray(QByteArray *ba){
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_5);
    out <<(quint32)ba->size();
    out <<(quint32)MESSAGE_BYTEARRAY;
    out << *ba;
    m_socket->write(block);
    m_socket->flush();
}

void SocketClient::sendImage(QImage* image){
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

void SocketClient::sendString(QString text){
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_5);
    //reserved for msg length
    out << (quint32)0;
    out << (quint32)Message_STRING;
    out << text;
    out.device()->seek(0);
    out << (quint32)(block.size() - sizeof(quint32));
    m_socket->write(block);
    m_socket->flush();
}

void SocketClient::sendWinId(WId winid){
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

void SocketClient::connectedCallBack(){
    qDebug()<<"socket is connected";
}

void SocketClient::disconnectedCallBack(){
    qDebug()<<"socket is disconnected";
}
