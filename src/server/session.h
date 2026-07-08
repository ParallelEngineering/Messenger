#ifndef MESSENGER_SESSION_H
#define MESSENGER_SESSION_H

#include "message.h"

#include <QDataStream>
#include <QObject>

class QTcpSocket;

class Session final : public QObject {
    Q_OBJECT

   public:
    explicit Session(QTcpSocket* socket, QObject* parent = nullptr);

    void sendMessage(const messenger::protocol::Message& message);
    [[nodiscard]] QString userName() const;
    [[nodiscard]] bool hasUserName() const;
    void setUserName(const QString& userName);

   signals:
    void messageReceived(const messenger::protocol::Message& message, Session* session);
    void disconnected(Session* session);

   private slots:
    void readAvailable();
    void handleDisconnected();
    void handleError();

   private:
    QTcpSocket* socket_;
    QDataStream stream_;
    QString userName_;
};

#endif  // MESSENGER_SESSION_H
