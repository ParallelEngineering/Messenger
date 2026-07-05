#include "server.h"

#include "session.h"

#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include <QTcpSocket>
#include <QtAlgorithms>

#include <utility>

using messenger::protocol::CurrentProtocolVersion;
using messenger::protocol::DefaultPort;
using messenger::protocol::Message;
using messenger::protocol::MessageType;

server& server::getInstance() {
    static server instance;
    return instance;
}

server::server() {
    connect(&tcpServer_, &QTcpServer::newConnection, this, &server::handleNewConnection);
    connect(&tcpServer_, &QTcpServer::acceptError, this, &server::handleAcceptError);
}

server::~server() {
    tcpServer_.close();
    qDeleteAll(sessions_);
    sessions_.clear();
}

bool server::listen(const QHostAddress& address, quint16 port) {
    if (tcpServer_.listen(address, port)) {
        qInfo() << "Messenger server listening on" << tcpServer_.serverAddress().toString()
                << tcpServer_.serverPort();
        return true;
    }

    qCritical() << "Could not start Messenger server:" << tcpServer_.errorString();
    return false;
}

void server::handleNewConnection() {
    while (tcpServer_.hasPendingConnections()) {
        auto* socket = tcpServer_.nextPendingConnection();
        auto* session = new Session(socket, this);
        sessions_.insert(session);

        connect(session, &Session::messageReceived, this, &server::handleMessageReceived);
        connect(session, &Session::disconnected, this, &server::handleSessionDisconnected);
    }
}

void server::handleAcceptError(QAbstractSocket::SocketError socketError) {
    qWarning() << "Server accept error:" << socketError << tcpServer_.errorString();
}

void server::handleMessageReceived(const Message& message, Session* session) {
    if (message.protocolVersion != CurrentProtocolVersion) {
        qWarning() << "Ignoring message with unsupported protocol version" << message.protocolVersion;
        return;
    }

    qInfo() << "Message from" << message.sender << "to" << message.recipient << ":" << message.text;

    if (message.messageType == static_cast<quint32>(MessageType::ChatMessage)) {
        for (auto* connectedSession : std::as_const(sessions_)) {
            connectedSession->sendMessage(message);
        }
    }

    Q_UNUSED(session)
}

void server::handleSessionDisconnected(Session* session) {
    sessions_.remove(session);
    session->deleteLater();
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    auto& serverInstance = server::getInstance();
    if (!serverInstance.listen(QHostAddress::Any, DefaultPort)) {
        return 1;
    }

    return app.exec();
}
