#ifndef MESSENGER_CLIENT_SESSION_H
#define MESSENGER_CLIENT_SESSION_H

#include "message_envelope.h"

#include <QDataStream>
#include <QObject>

class QTcpSocket;

class ClientSession final : public QObject {
    Q_OBJECT

   public:
    explicit ClientSession(QTcpSocket* socket, QObject* parent = nullptr);

    void sendMessage(const messenger::protocol::MessageEnvelope& message);

   signals:
    void messageReceived(const messenger::protocol::MessageEnvelope& message, ClientSession* session);
    void disconnected(ClientSession* session);

   private slots:
    void readAvailable();
    void handleDisconnected();
    void handleError();

   private:
    QTcpSocket* socket_;
    QDataStream stream_;
};

#endif  // MESSENGER_CLIENT_SESSION_H
