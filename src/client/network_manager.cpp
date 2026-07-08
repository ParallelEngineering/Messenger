#include "network_manager.h"

#include <QAbstractSocket>
#include <QDateTime>

using messenger::protocol::CurrentProtocolVersion;
using messenger::protocol::DataStreamVersion;
using messenger::protocol::DefaultPort;
using messenger::protocol::Message;
using messenger::protocol::MessageType;
using messenger::protocol::ReadBufferSize;

NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent), stream_(&socket_), statusText_(tr("Disconnected")) {
    socket_.setReadBufferSize(ReadBufferSize);
    stream_.setVersion(DataStreamVersion);

    connect(&socket_, &QTcpSocket::connected, this, &NetworkManager::handleConnected);
    connect(&socket_, &QTcpSocket::disconnected, this, &NetworkManager::handleDisconnected);
    connect(&socket_, &QTcpSocket::errorOccurred, this, &NetworkManager::handleError);
    connect(&socket_, &QTcpSocket::readyRead, this, &NetworkManager::readAvailable);
}

bool NetworkManager::connected() const {
    return socket_.state() == QAbstractSocket::ConnectedState;
}

QString NetworkManager::statusText() const {
    return statusText_;
}

int NetworkManager::defaultPort() const {
    return DefaultPort;
}

void NetworkManager::connectToServer(const QString& host, quint16 port) {
    const auto trimmedHost = host.trimmed();
    if (trimmedHost.isEmpty()) {
        const auto message = tr("Host must not be empty.");
        setStatusText(message);
        emit connectionError(message);
        return;
    }

    const auto previousConnected = connected();
    if (socket_.state() != QAbstractSocket::UnconnectedState) {
        socket_.abort();
    }

    setStatusText(tr("Connecting to %1:%2 ...").arg(trimmedHost).arg(port));
    emitConnectedIfChanged(previousConnected);
    socket_.connectToHost(trimmedHost, port);
}

void NetworkManager::disconnectFromServer() {
    const auto previousConnected = connected();

    if (socket_.state() == QAbstractSocket::UnconnectedState) {
        setStatusText(tr("Disconnected"));
        emitConnectedIfChanged(previousConnected);
        return;
    }

    setStatusText(tr("Disconnecting ..."));
    socket_.disconnectFromHost();
}

void NetworkManager::sendChatMessage(const QString& text) {
    if (!connected()) {
        const auto message = tr("Not connected to a server.");
        setStatusText(message);
        emit connectionError(message);
        return;
    }

    Message message;
    message.messageType = static_cast<quint32>(MessageType::ChatMessage);
    message.text = text;
    message.timestamp = QDateTime::currentDateTimeUtc();

    QDataStream out(&socket_);
    out.setVersion(DataStreamVersion);
    out << message;
    socket_.flush();
}

void NetworkManager::handleConnected() {
    setStatusText(tr("Connected"));
    emit connectedChanged();
}

void NetworkManager::handleDisconnected() {
    setStatusText(tr("Disconnected"));
    emit connectedChanged();
}

void NetworkManager::handleError() {
    if (socket_.error() == QAbstractSocket::RemoteHostClosedError) {
        return;
    }

    const auto message = socket_.errorString();
    setStatusText(message);
    emit connectionError(message);
}

void NetworkManager::readAvailable() {
    while (socket_.bytesAvailable() > 0) {
        stream_.startTransaction();

        Message message;
        stream_ >> message;

        if (!stream_.commitTransaction()) {
            return;
        }

        if (message.protocolVersion != CurrentProtocolVersion) {
            const auto errorMessage = tr("Unsupported protocol version: %1").arg(message.protocolVersion);
            setStatusText(errorMessage);
            emit connectionError(errorMessage);
            socket_.disconnectFromHost();
            return;
        }

        emit messageReceived(message.text);
    }
}

void NetworkManager::setStatusText(const QString& statusText) {
    if (statusText_ == statusText) {
        return;
    }

    statusText_ = statusText;
    emit statusTextChanged();
}

void NetworkManager::emitConnectedIfChanged(bool previousConnected) {
    if (previousConnected != connected()) {
        emit connectedChanged();
    }
}
