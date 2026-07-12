#include "session.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QDebug>
#include <QTcpSocket>

using messenger::protocol::AuthenticationTimeoutMs;
using messenger::protocol::CurrentProtocolVersion;
using messenger::protocol::DataStreamVersion;
using messenger::protocol::Message;
using messenger::protocol::MessageType;
using messenger::protocol::ReadBufferSize;

Session::Session(QTcpSocket* socket, QObject* parent)
    : QObject(parent), socket_(socket), stream_(socket) {
    socket_->setParent(this);
    socket_->setReadBufferSize(ReadBufferSize);
    stream_.setVersion(DataStreamVersion);

    connect(socket_, &QTcpSocket::readyRead, this, &Session::readAvailable);
    connect(socket_, &QTcpSocket::disconnected, this, &Session::handleDisconnected);
    connect(socket_, &QTcpSocket::errorOccurred, this, &Session::handleError);

    authenticationDeadline_.setRemainingTime(AuthenticationTimeoutMs);
    authenticationTimer_.setSingleShot(true);
    authenticationTimer_.start(AuthenticationTimeoutMs);
    connect(&authenticationTimer_, &QTimer::timeout, this, [this]() {
        if (!isAuthenticated()) {
            qWarning() << "Client authentication timed out for"
                       << socket_->peerAddress().toString();
            rejectAuthentication(QStringLiteral("Authentication timed out."));
        }
    });

    qInfo() << "Client connected from" << socket_->peerAddress().toString() << socket_->peerPort();
}

void Session::sendMessage(const Message& message) {
    if (socket_->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QDataStream out(socket_);
    out.setVersion(DataStreamVersion);
    out << message;
    socket_->flush();
}

void Session::beginAuthentication(int userId, const QString& userName, const PublicKey& publicKey,
                                  const QByteArray& authenticationId, const QByteArray& clientNonce,
                                  const QByteArray& serverNonce) {
    if (authenticationState_ != AuthenticationState::AwaitingHello) {
        return;
    }

    userId_ = userId;
    userName_ = userName.trimmed();
    authenticationPublicKey_ = publicKey;
    authenticationId_ = authenticationId;
    clientNonce_ = clientNonce;
    serverNonce_ = serverNonce;
    authenticationState_ = AuthenticationState::AwaitingProof;
}

void Session::completeAuthentication() {
    if (authenticationState_ != AuthenticationState::AwaitingProof) {
        return;
    }

    authenticationState_ = AuthenticationState::Authenticated;
    authenticationTimer_.stop();
    authenticationPublicKey_ = {};
    authenticationId_.clear();
    clientNonce_.clear();
    serverNonce_.clear();
}

void Session::rejectAuthentication(const QString& reason, MessageType messageType) {
    if (authenticationState_ == AuthenticationState::Rejected) {
        return;
    }

    authenticationState_ = AuthenticationState::Rejected;
    authenticationTimer_.stop();
    authenticationPublicKey_ = {};
    authenticationId_.clear();
    clientNonce_.clear();
    serverNonce_.clear();

    Message failure;
    failure.messageType = static_cast<quint32>(messageType);
    failure.text = reason;
    sendMessage(failure);
    socket_->disconnectFromHost();
}

void Session::disconnectFromHost() { socket_->disconnectFromHost(); }

Session::AuthenticationState Session::authenticationState() const { return authenticationState_; }

bool Session::isAuthenticated() const {
    return authenticationState_ == AuthenticationState::Authenticated;
}

bool Session::authenticationExpired() const { return authenticationDeadline_.hasExpired(); }

int Session::userId() const { return userId_; }

QString Session::userName() const { return userName_; }

const PublicKey& Session::authenticationPublicKey() const { return authenticationPublicKey_; }

const QByteArray& Session::authenticationId() const { return authenticationId_; }

const QByteArray& Session::clientNonce() const { return clientNonce_; }

const QByteArray& Session::serverNonce() const { return serverNonce_; }

QString Session::peerAddress() const { return socket_->peerAddress().toString(); }

void Session::readAvailable() {
    while (socket_->bytesAvailable() > 0) {
        stream_.startTransaction();

        Message message;
        stream_ >> message;

        if (!stream_.commitTransaction()) {
            return;
        }

        if (message.protocolVersion != CurrentProtocolVersion) {
            qWarning() << "Unsupported protocol version" << message.protocolVersion << "from"
                       << socket_->peerAddress().toString();
            socket_->disconnectFromHost();
            return;
        }

        emit messageReceived(message, this);
    }
}

void Session::handleDisconnected() {
    qInfo() << "Client disconnected from" << socket_->peerAddress().toString()
            << socket_->peerPort();
    emit disconnected(this);
}

void Session::handleError() {
    if (socket_->error() == QAbstractSocket::RemoteHostClosedError) {
        return;
    }

    qWarning() << "Client socket error:" << socket_->errorString();
}
