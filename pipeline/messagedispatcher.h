#ifndef SOCKETCLIENT_H
#define SOCKETCLIENT_H

#include <QDataStream>
#include <QObject>
#include <QtGui>
#include <functional>
#include <memory>

#include "utils/messagetype.h"

class QLocalSocket;

class MessageDispatcher : public QObject {
    Q_OBJECT
public:
    MessageDispatcher(const QString& socketName, QObject* parent = nullptr);
    bool connectServer();
    void close();

    void sendImage(QImage* image);
    void sendString(const QString&);
    void sendWinId(WId);
    void sendByteArray(QByteArray* ba);

public Q_SLOTS:
    void connectedCallBack();
    void disconnectedCallBack();
    void messageReceived();

Q_SIGNALS:
    void actionReceived(PipelineAction action);

private:
    QString m_socketName {};
    QLocalSocket* m_socket { nullptr };
};

#endif // SOCKETCLIENT_H
