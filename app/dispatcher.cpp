/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "dispatcher.h"
#include "messagetype.h"
#include <QDataStream>
#include <QImage>
#include <QLocalServer>
#include <QLocalSocket>
#include <QtEndian>

Dispatcher::Dispatcher(const QString& name, QObject* parent)
    : QObject(parent)
    , m_name(name)
{
    m_server = new QLocalServer(this);
    QLocalServer::removeServer(m_name);
    if (!m_server->listen(m_name)) {
        qDebug() << "unable to start the server: "
                 << m_name << ":" << m_server->errorString();
        close();
    }
    connect(m_server, SIGNAL(newConnection()), this, SLOT(ConnectionArrived()));
}
Dispatcher::~Dispatcher()
{
    close();
}

void Dispatcher::ConnectionArrived()
{
    QLocalSocket* clientSocket = m_server->nextPendingConnection();
    qintptr sp = clientSocket->socketDescriptor();

    qDebug() << "new connection arrived:" << sp;
    connect(clientSocket, SIGNAL(disconnected()), clientSocket, SLOT(deleteLater()));
    connect(clientSocket, &QLocalSocket::readyRead, this, [this, sp]() { this->MessageRecieved(sp); });
    m_clientSocket.insert(sp, clientSocket);

    QDataStream* dataStream = new QDataStream();
    dataStream->setDevice(clientSocket);
    dataStream->setVersion(QDataStream::Qt_5_5);

    m_dataStream.insert(sp, dataStream);
    m_curMsgLength.insert(sp, 0);
}

void Dispatcher::sendAction(PipelineAction action)
{
    for (auto& clientSocket : m_clientSocket) {
        QDataStream out(clientSocket);
        out.setVersion(QDataStream::Qt_5_5);
        out << (quint32)sizeof(quint32) * 2;
        out << (quint32)MESSAGE_ACTION;
        out << (quint32)MESSAGE_STOP;
        clientSocket->waitForBytesWritten();
    }
}

void Dispatcher::MessageRecieved(qintptr sp)
{
    while (!m_dataStream[sp]->atEnd()) {
        /*
     * message strcuture:| Message header | Message content
     *                   | Length | Type  | Data
     *                   | 4bytes | 4bytes| Length - 4 bytes

     */
        if (m_curMsgLength[sp] == 0) {
            // read message length
            if (m_clientSocket[sp]->bytesAvailable() < (int)sizeof(quint32)) {
                return;
            } else {
                *m_dataStream[sp] >> m_curMsgLength[sp];
            }
        }
        if (m_clientSocket[sp]->bytesAvailable() < m_curMsgLength[sp] || m_dataStream[sp]->atEnd()) {
            return;
        } else {
            quint32 msgType;
            *m_dataStream[sp] >> msgType;

            switch (static_cast<MessageType>(msgType)) {
            case MESSAGE_IMAGE: {
                QImage* image = new QImage();
                *m_dataStream[sp] >> *image;
                Q_EMIT ImageReceived(sp, image);
                break;
            }
            case MESSAGE_STRING: {
                QString text;
                *m_dataStream[sp] >> text;
                Q_EMIT StringReceived(sp, text);
                break;
            }
            case MESSAGE_WINID: {
                WId wid;
                *m_dataStream[sp] >> wid;
                Q_EMIT WIDReceived(sp, wid);
                break;
            }
            case MESSAGE_BYTEARRAY: {
                QByteArray* ba = new QByteArray();
                *m_dataStream[sp] >> *ba;
                Q_EMIT ByteArrayReceived(sp, ba);
                break;
            }

            case MESSAGE_ACTION: {
                quint32 data;
                *m_dataStream[sp] >> data;
                PipelineAction action = static_cast<PipelineAction>(data);
                Q_EMIT ActionReceived(sp, action);
                break;
            }

            case MESSAGE_UNKNOWN:
            default:
                qDebug() << "unknown message type, try to skip following content size" << m_curMsgLength[sp];
                m_dataStream[sp]->skipRawData(m_curMsgLength[sp]);
            }
            m_curMsgLength[sp] = 0;
        }
    }
}

void Dispatcher::close()
{
    m_clientSocket.clear();

    for (auto i = m_dataStream.begin(); i != m_dataStream.end(); i++) {
        delete i.value();
    }
    m_dataStream.clear();

    m_server->close();
}
