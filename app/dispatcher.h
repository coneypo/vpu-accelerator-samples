/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#include <QMap>
#include <QObject>
#include <QtGui>
#include "utils/messagetype.h"

class QLocalServer;
class QLocalSocket;

class Dispatcher : public QObject {
    Q_OBJECT
public:
    explicit Dispatcher(const QString& name, QObject* parent = 0);
    virtual ~Dispatcher();

    void close();
    void MessageRecieved(qintptr);

Q_SIGNALS:
    void WIDReceived(qintptr sd, WId wid);
    void ImageReceived(qintptr sd, QImage* image);
    void StringReceived(qintptr sd, QString text);
    void ByteArrayReceived(qintptr sd, QByteArray* ba);
    void ActionReceived(qintptr sd, PipelineAction action);

public Q_SLOTS:
    void ConnectionArrived();
    void sendAction(PipelineAction action);
    //void disconnectedCallBack();

private:
    QString m_name;
    QLocalServer* m_server;
    QMap<qintptr, QLocalSocket*> m_clientSocket;
    QMap<qintptr, QDataStream*> m_dataStream;
    QMap<qintptr, quint32> m_curMsgLength;
};

#endif // SOCKETSERVER_H
