#ifndef SOCKETCLIENT_H
#define SOCKETCLIENT_H

#include <QObject>
#include <QtGui>
#include <QDataStream>
#include <functional>
#include <memory>

class QLocalSocket;

class SocketClient: public QObject
{
    Q_OBJECT
public:
    SocketClient(QString socketName, QObject* parent=nullptr);
    bool connectServer();
    void close();

    void sendImage(QImage* image);
    void sendString(QString);
    void sendWinId(WId);
    void sendByteArray(QByteArray* ba);


public Q_SLOTS:
    void connectedCallBack();
    void disconnectedCallBack();

private:
    QString m_socketName;
    QLocalSocket* m_socket;
};

#endif // SOCKETCLIENT_H
