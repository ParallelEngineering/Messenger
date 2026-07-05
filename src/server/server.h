#ifndef MESSENGER_SERVER_H
#define MESSENGER_SERVER_H

#include "message_envelope.h"

#include <QAbstractSocket>
#include <QObject>
#include <QSet>
#include <QTcpServer>

class ClientSession;
class QHostAddress;

class server final : public QObject {
    Q_OBJECT

   public:
    static server& getInstance();
    ~server() override;

    bool listen(const QHostAddress& address, quint16 port);

    server(const server&) = delete;
    server& operator=(const server&) = delete;
    server(server&&) = delete;
    server& operator=(server&&) = delete;

   private slots:
    void handleNewConnection();
    void handleAcceptError(QAbstractSocket::SocketError socketError);
    void handleMessageReceived(const messenger::protocol::MessageEnvelope& message, ClientSession* session);
    void handleSessionDisconnected(ClientSession* session);

   private:
    server();

    QTcpServer tcpServer_;
    QSet<ClientSession*> sessions_;
};

#endif  // MESSENGER_SERVER_H
