#ifndef SOCKETCLIENT_H
#define SOCKETCLIENT_H

#include <QDataStream>
#include <QObject>
#include <QtGui>
#include <atomic>
#include <functional>
#include <memory>

#include "utils/messagetype.h"

class QLocalSocket;

class AppConnector : public QObject {
    Q_OBJECT
public:
    AppConnector(QString socketName, QObject* parent = nullptr);
    bool connectServer();
    void close();

    void sendImage(QImage* image);
    void sendString(QString);
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
    std::atomic<bool> m_stop { false };
};

#endif // SOCKETCLIENT_H
