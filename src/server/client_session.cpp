#include "client_session.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QTcpSocket>
#include <QDebug>

using messenger::protocol::CurrentProtocolVersion;
using messenger::protocol::DataStreamVersion;
using messenger::protocol::MessageEnvelope;
using messenger::protocol::ReadBufferSize;

ClientSession::ClientSession(QTcpSocket* socket, QObject* parent)
    : QObject(parent), socket_(socket), stream_(socket) {
    socket_->setParent(this);
    socket_->setReadBufferSize(ReadBufferSize);
    stream_.setVersion(DataStreamVersion);

    connect(socket_, &QTcpSocket::readyRead, this, &ClientSession::readAvailable);
    connect(socket_, &QTcpSocket::disconnected, this, &ClientSession::handleDisconnected);
    connect(socket_, &QTcpSocket::errorOccurred, this, &ClientSession::handleError);

    qInfo() << "Client connected from" << socket_->peerAddress().toString() << socket_->peerPort();
}

void ClientSession::sendMessage(const MessageEnvelope& message) {
    if (socket_->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QDataStream out(socket_);
    out.setVersion(DataStreamVersion);
    out << message;
    socket_->flush();
}

void ClientSession::readAvailable() {
    while (socket_->bytesAvailable() > 0) {
        stream_.startTransaction();

        MessageEnvelope message;
        stream_ >> message;

        if (!stream_.commitTransaction()) {
            return;
        }

        if (message.protocolVersion != CurrentProtocolVersion) {
            qWarning() << "Unsupported protocol version" << message.protocolVersion
                       << "from" << socket_->peerAddress().toString();
            socket_->disconnectFromHost();
            return;
        }

        emit messageReceived(message, this);
    }
}

void ClientSession::handleDisconnected() {
    qInfo() << "Client disconnected from" << socket_->peerAddress().toString() << socket_->peerPort();
    emit disconnected(this);
}

void ClientSession::handleError() {
    if (socket_->error() == QAbstractSocket::RemoteHostClosedError) {
        return;
    }

    qWarning() << "Client socket error:" << socket_->errorString();
}
