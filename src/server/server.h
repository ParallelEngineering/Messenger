#ifndef MESSENGER_SERVER_H
#define MESSENGER_SERVER_H

#include <QAbstractSocket>
#include <QObject>
#include <QSet>
#include <QTcpServer>

#include "Storage/message_store.h"
#include "message.h"

class Session;
class QHostAddress;

class server final : public QObject {
    Q_OBJECT

   public:
    static server& getInstance();
    ~server() override;

    bool listen(const QHostAddress& address, quint16 port);
    void shutdown();

    server(const server&) = delete;
    server& operator=(const server&) = delete;
    server(server&&) = delete;
    server& operator=(server&&) = delete;

   private slots:
    void handleNewConnection();
    void handleAcceptError(QAbstractSocket::SocketError socketError);
    void handleMessageReceived(const messenger::protocol::Message& message, Session* session);
    void handleSessionDisconnected(Session* session);

   private:
    server();

    QTcpServer tcpServer_;
    QSet<Session*> sessions_;
    MessageStore messageStore_;
};

#endif  // MESSENGER_SERVER_H
