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

namespace {

bool isValidChatMessage(const Message& message) {
    return message.messageType == static_cast<quint32>(MessageType::ChatMessage)
           && !message.senderName.trimmed().isEmpty()
           && !message.text.trimmed().isEmpty()
           && message.timestamp.isValid();
}

}  // namespace

server& server::getInstance() {
    static server instance;
    return instance;
}

server::server() {
    connect(&tcpServer_, &QTcpServer::newConnection, this, &server::handleNewConnection);
    connect(&tcpServer_, &QTcpServer::acceptError, this, &server::handleAcceptError);
}

server::~server() {
    shutdown();
}

void server::shutdown() {
    tcpServer_.close();
    qDeleteAll(sessions_);
    sessions_.clear();
    if (QCoreApplication::instance() != nullptr) {
        messageStore_.close();
    }
}

bool server::listen(const QHostAddress& address, quint16 port) {
    if (!messageStore_.initialize()) {
        qCritical() << "Could not initialize message storage";
        return false;
    }

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

        const auto previousMessages = messageStore_.loadMessages();
        for (const auto& message : previousMessages) {
            session->sendMessage(message);
        }
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

    if (message.messageType == static_cast<quint32>(MessageType::ChatMessage)) {
        if (!isValidChatMessage(message)) {
            qWarning() << "Ignoring invalid chat message from session";
            return;
        }

        const auto senderName = message.senderName.trimmed();

        if (!messageStore_.hasUser(senderName)) {
            qWarning() << "Ignoring chat message from unknown user:" << senderName;
            return;
        }

        // TODO: Replace this self-declared session binding with login and authentication
        // once user creation/authentication exist.
        if (!session->hasUserName()) {
            session->setUserName(senderName);
        } else if (session->userName() != senderName) {
            qWarning() << "Ignoring chat message with sender mismatch. Session user:"
                       << session->userName() << "message sender:" << senderName;
            return;
        }

        Message verifiedMessage = message;
        verifiedMessage.senderName = senderName;

        qInfo() << "Message from" << verifiedMessage.senderName << ":" << verifiedMessage.text;

        if (!messageStore_.saveMessage(verifiedMessage)) {
            qWarning() << "Message was not persisted";
            return;
        }

        for (auto* connectedSession : std::as_const(sessions_)) {
            connectedSession->sendMessage(verifiedMessage);
        }
    }
}

void server::handleSessionDisconnected(Session* session) {
    sessions_.remove(session);
    session->deleteLater();
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Messenger"));
    QCoreApplication::setOrganizationName(QStringLiteral("ParallelEngineering"));

    auto& serverInstance = server::getInstance();
    if (!serverInstance.listen(QHostAddress::Any, DefaultPort)) {
        serverInstance.shutdown();
        return 1;
    }

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &serverInstance, &server::shutdown);

    return app.exec();
}
