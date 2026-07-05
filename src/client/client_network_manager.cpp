#include "client_network_manager.h"

#include <QAbstractSocket>
#include <QDateTime>

using messenger::protocol::CurrentProtocolVersion;
using messenger::protocol::DataStreamVersion;
using messenger::protocol::DefaultPort;
using messenger::protocol::MessageEnvelope;
using messenger::protocol::MessageType;
using messenger::protocol::ReadBufferSize;

ClientNetworkManager::ClientNetworkManager(QObject* parent)
    : QObject(parent), stream_(&socket_), statusText_(tr("Disconnected")) {
    socket_.setReadBufferSize(ReadBufferSize);
    stream_.setVersion(DataStreamVersion);

    connect(&socket_, &QTcpSocket::connected, this, &ClientNetworkManager::handleConnected);
    connect(&socket_, &QTcpSocket::disconnected, this, &ClientNetworkManager::handleDisconnected);
    connect(&socket_, &QTcpSocket::errorOccurred, this, &ClientNetworkManager::handleError);
    connect(&socket_, &QTcpSocket::readyRead, this, &ClientNetworkManager::readAvailable);
}

bool ClientNetworkManager::connected() const {
    return socket_.state() == QAbstractSocket::ConnectedState;
}

QString ClientNetworkManager::statusText() const {
    return statusText_;
}

int ClientNetworkManager::defaultPort() const {
    return DefaultPort;
}

void ClientNetworkManager::connectToServer(const QString& host, quint16 port) {
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

void ClientNetworkManager::disconnectFromServer() {
    const auto previousConnected = connected();

    if (socket_.state() == QAbstractSocket::UnconnectedState) {
        setStatusText(tr("Disconnected"));
        emitConnectedIfChanged(previousConnected);
        return;
    }

    setStatusText(tr("Disconnecting ..."));
    socket_.disconnectFromHost();
}

void ClientNetworkManager::sendChatMessage(const QString& sender, const QString& recipient, const QString& text) {
    if (!connected()) {
        const auto message = tr("Not connected to a server.");
        setStatusText(message);
        emit connectionError(message);
        return;
    }

    MessageEnvelope message;
    message.messageType = static_cast<quint32>(MessageType::ChatMessage);
    message.sender = sender;
    message.recipient = recipient;
    message.text = text;
    message.timestamp = QDateTime::currentDateTimeUtc();

    QDataStream out(&socket_);
    out.setVersion(DataStreamVersion);
    out << message;
    socket_.flush();
}

void ClientNetworkManager::handleConnected() {
    setStatusText(tr("Connected"));
    emit connectedChanged();
}

void ClientNetworkManager::handleDisconnected() {
    setStatusText(tr("Disconnected"));
    emit connectedChanged();
}

void ClientNetworkManager::handleError() {
    if (socket_.error() == QAbstractSocket::RemoteHostClosedError) {
        return;
    }

    const auto message = socket_.errorString();
    setStatusText(message);
    emit connectionError(message);
}

void ClientNetworkManager::readAvailable() {
    while (socket_.bytesAvailable() > 0) {
        stream_.startTransaction();

        MessageEnvelope message;
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

        emit messageReceived(message.sender, message.recipient, message.text);
    }
}

void ClientNetworkManager::setStatusText(const QString& statusText) {
    if (statusText_ == statusText) {
        return;
    }

    statusText_ = statusText;
    emit statusTextChanged();
}

void ClientNetworkManager::emitConnectedIfChanged(bool previousConnected) {
    if (previousConnected != connected()) {
        emit connectedChanged();
    }
}
