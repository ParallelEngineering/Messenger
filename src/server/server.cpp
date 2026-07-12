#include "server.h"

#include "session.h"
#include "keyPair.h"
#include "signature.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QHostAddress>
#include <QTcpSocket>
#include <QtAlgorithms>

#include <utility>
#include <cstdint>
#include <exception>
#include <optional>
#include <vector>

using messenger::protocol::CurrentProtocolVersion;
using messenger::protocol::DefaultPort;
using messenger::protocol::Message;
using messenger::protocol::MessageType;

namespace {

bool isValidChatMessage(const Message& message) {
    return message.messageType == static_cast<quint32>(MessageType::ChatMessage)
           && !message.text.trimmed().isEmpty()
           && message.text.size() <= messenger::protocol::MaximumMessageSize;
}

std::vector<std::uint8_t> toByteVector(const QByteArray& bytes) {
    const auto* begin = reinterpret_cast<const std::uint8_t*>(bytes.constData());
    return {begin, begin + bytes.size()};
}

std::optional<PublicKey> deserializePublicKey(const QByteArray& serializedKey) {
    try {
        PublicKey publicKey;
        const auto bytes = toByteVector(serializedKey);
        const operations::BigInt one(1);
        if (!keyPair::s_deserialize(bytes, publicKey.n, publicKey.e)
            || publicKey.n <= one
            || publicKey.e <= one
            || publicKey.serialize() != bytes) {
            return std::nullopt;
        }
        return publicKey;
    } catch (const std::exception&) {
        return std::nullopt;
    }
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

    const auto messageType = static_cast<MessageType>(message.messageType);

    if (session->authenticationState() == Session::AuthenticationState::AwaitingHello) {
        if (messageType != MessageType::AuthHello) {
            qWarning() << "Rejecting unexpected message before AuthHello";
            session->rejectAuthentication();
            return;
        }

        const auto requestedUserName = message.senderName.trimmed();
        if (requestedUserName.isEmpty()
            || requestedUserName.size() > messenger::protocol::MaximumUserNameSize
            || message.clientNonce.size() != messenger::protocol::AuthenticationNonceSize) {
            qWarning() << "Rejecting malformed AuthHello";
            session->rejectAuthentication();
            return;
        }

        const auto user = messageStore_.findUserForAuthentication(requestedUserName);
        if (!user) {
            qWarning() << "Authentication requested for unknown user:" << requestedUserName;
            session->rejectAuthentication();
            return;
        }

        const auto publicKey = deserializePublicKey(user->publicKey);
        if (!publicKey) {
            qWarning() << "Stored public key is invalid for user:" << user->userName;
            session->rejectAuthentication();
            return;
        }

        const auto authenticationId = messenger::protocol::generateSecureRandomBytes(
            messenger::protocol::AuthenticationIdSize);
        const auto serverNonce = messenger::protocol::generateSecureRandomBytes(
            messenger::protocol::AuthenticationNonceSize);
        session->beginAuthentication(user->userId,
                                     user->userName,
                                     *publicKey,
                                     authenticationId,
                                     message.clientNonce,
                                     serverNonce);

        Message challenge;
        challenge.messageType = static_cast<quint32>(MessageType::AuthChallenge);
        challenge.authenticationId = authenticationId;
        challenge.serverNonce = serverNonce;
        session->sendMessage(challenge);
        qInfo() << "Sent authentication challenge for user" << user->userName;
        return;
    }

    if (session->authenticationState() == Session::AuthenticationState::AwaitingProof) {
        if (messageType != MessageType::AuthProof
            || session->authenticationExpired()
            || message.authenticationId.size() != messenger::protocol::AuthenticationIdSize
            || message.authenticationId != session->authenticationId()
            || message.signature.isEmpty()) {
            qWarning() << "Rejecting malformed or expired AuthProof for" << session->userName();
            session->rejectAuthentication();
            return;
        }

        const auto transcript = messenger::protocol::authenticationTranscript(
            session->userName(),
            session->authenticationId(),
            session->clientNonce(),
            session->serverNonce());
        const auto digest = QCryptographicHash::hash(transcript, QCryptographicHash::Sha256);
        const auto signatureValid = core::signature::verifyDigest(
            session->authenticationPublicKey(),
            toByteVector(digest),
            toByteVector(message.signature));
        if (!signatureValid) {
            qWarning() << "Authentication signature verification failed for" << session->userName();
            session->rejectAuthentication();
            return;
        }

        session->completeAuthentication();
        qInfo() << "Authenticated user" << session->userName();

        Message success;
        success.messageType = static_cast<quint32>(MessageType::AuthSuccess);
        success.senderName = session->userName();
        success.text = QStringLiteral("Authentication successful.");
        session->sendMessage(success);

        const auto previousMessages = messageStore_.loadMessages();
        for (const auto& previousMessage : previousMessages) {
            session->sendMessage(previousMessage);
        }
        return;
    }

    if (!session->isAuthenticated()) {
        session->disconnectFromHost();
        return;
    }

    if (messageType != MessageType::ChatMessage || !isValidChatMessage(message)) {
        qWarning() << "Disconnecting authenticated session after invalid message from"
                   << session->userName();
        session->disconnectFromHost();
        return;
    }

    Message verifiedMessage = message;
    verifiedMessage.senderName = session->userName();
    verifiedMessage.text = message.text.trimmed();
    verifiedMessage.timestamp = QDateTime::currentDateTimeUtc();
    verifiedMessage.authenticationId.clear();
    verifiedMessage.clientNonce.clear();
    verifiedMessage.serverNonce.clear();
    verifiedMessage.signature.clear();

    qInfo() << "Message from" << verifiedMessage.senderName << ":" << verifiedMessage.text;

    if (!messageStore_.saveMessage(verifiedMessage)) {
        qWarning() << "Message was not persisted";
        return;
    }

    for (auto* connectedSession : std::as_const(sessions_)) {
        if (connectedSession->isAuthenticated()) {
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
