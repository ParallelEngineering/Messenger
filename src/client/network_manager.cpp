#include "network_manager.h"

#include <QAbstractSocket>
#include <QCryptographicHash>
#include <QDateTime>
#include <QTimer>
#include <QtGlobal>
#include <cstdint>
#include <vector>

#include "connection_store.h"
#include "signature.h"

using messenger::protocol::CurrentProtocolVersion;
using messenger::protocol::DataStreamVersion;
using messenger::protocol::DefaultPort;
using messenger::protocol::Message;
using messenger::protocol::MessageType;
using messenger::protocol::ReadBufferSize;

namespace {

std::vector<std::uint8_t> toByteVector(const QByteArray& bytes) {
    const auto* begin = reinterpret_cast<const std::uint8_t*>(bytes.constData());
    return {begin, begin + bytes.size()};
}

QByteArray toByteArray(const std::vector<std::uint8_t>& bytes) {
    return QByteArray(reinterpret_cast<const char*>(bytes.data()),
                      static_cast<qsizetype>(bytes.size()));
}

}  // namespace

NetworkManager::NetworkManager(ConnectionStore* connectionStore, QObject* parent)
    : QObject(parent),
      connectionStore_(connectionStore),
      stream_(&socket_),
      statusText_(tr("Disconnected")),
      userName_(QStringLiteral("anonymous")) {
    socket_.setReadBufferSize(ReadBufferSize);
    stream_.setVersion(DataStreamVersion);

    connect(&socket_, &QTcpSocket::connected, this, &NetworkManager::handleConnected);
    connect(&socket_, &QTcpSocket::disconnected, this, &NetworkManager::handleDisconnected);
    connect(&socket_, &QTcpSocket::errorOccurred, this, &NetworkManager::handleError);
    connect(&socket_, &QTcpSocket::readyRead, this, &NetworkManager::readAvailable);

    authenticationTimer_.setSingleShot(true);
    connect(&authenticationTimer_, &QTimer::timeout, this, [this]() {
        if (!connected() && connectionState_ != ConnectionState::Disconnected) {
            failAuthentication(tr("Authentication timed out."));
        }
    });
}

bool NetworkManager::connected() const {
    return connectionState_ == ConnectionState::Authenticated;
}

bool NetworkManager::busy() const {
    return connectionState_ != ConnectionState::Disconnected && !connected();
}

QString NetworkManager::statusText() const { return statusText_; }

QString NetworkManager::userName() const { return userName_; }

int NetworkManager::defaultPort() const { return DefaultPort; }

void NetworkManager::setUserName(const QString& userName) {
    const auto trimmedUserName = userName.trimmed();
    if (userName_ == trimmedUserName) {
        return;
    }

    userName_ = trimmedUserName;
    emit userNameChanged();
}

void NetworkManager::connectToServer(const QString& host, quint16 port) {
    const auto trimmedHost = host.trimmed();
    if (trimmedHost.isEmpty()) {
        const auto message = tr("Host must not be empty.");
        setStatusText(message);
        emit connectionError(message);
        return;
    }

    if (userName_.trimmed().isEmpty()) {
        const auto message = tr("User name must not be empty.");
        setStatusText(message);
        emit connectionError(message);
        return;
    }

    const auto* selectedKeyPair = connectionStore_ ? connectionStore_->currentKeyPair() : nullptr;
    if (selectedKeyPair == nullptr) {
        const auto message = tr("Select a valid RSA key pair before connecting.");
        setStatusText(message);
        emit connectionError(message);
        return;
    }

    if (socket_.state() != QAbstractSocket::UnconnectedState) {
        socket_.abort();
    }

    clearAuthenticationData();
    authenticationKeyPair_ = *selectedKeyPair;
    authenticationUserName_ = userName_.trimmed();
    preserveStatusOnDisconnect_ = false;
    userRequestedDisconnect_ = false;
    setConnectionState(ConnectionState::Connecting);
    setStatusText(tr("Connecting to %1:%2 ...").arg(trimmedHost).arg(port));
    socket_.connectToHost(trimmedHost, port);
}

void NetworkManager::disconnectFromServer() {
    if (socket_.state() == QAbstractSocket::UnconnectedState) {
        clearAuthenticationData();
        setConnectionState(ConnectionState::Disconnected);
        setStatusText(tr("Disconnected"));
        return;
    }

    userRequestedDisconnect_ = true;
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

    const auto senderName = userName_.trimmed();
    if (senderName.isEmpty()) {
        const auto message = tr("User name must not be empty.");
        setStatusText(message);
        emit connectionError(message);
        return;
    }

    Message message;
    message.messageType = static_cast<quint32>(MessageType::ChatMessage);
    message.senderName = senderName;
    message.text = text;
    message.timestamp = QDateTime::currentDateTimeUtc();

    QDataStream out(&socket_);
    out.setVersion(DataStreamVersion);
    out << message;
    socket_.flush();
}

void NetworkManager::handleConnected() {
    setConnectionState(ConnectionState::AwaitingChallenge);
    setStatusText(tr("Connected to server. Starting authentication ..."));
    authenticationTimer_.start(messenger::protocol::AuthenticationTimeoutMs);
    sendAuthenticationHello();
}

void NetworkManager::handleDisconnected() {
    const auto wasAuthenticated = connected();
    const auto wasUserRequested = userRequestedDisconnect_;
    clearAuthenticationData();
    setConnectionState(ConnectionState::Disconnected);

    if (wasAuthenticated || wasUserRequested || !preserveStatusOnDisconnect_) {
        setStatusText(wasAuthenticated || wasUserRequested
                          ? tr("Disconnected")
                          : tr("Connection closed before authentication completed."));
    }

    preserveStatusOnDisconnect_ = false;
    userRequestedDisconnect_ = false;
}

void NetworkManager::handleError() {
    if (socket_.error() == QAbstractSocket::RemoteHostClosedError) {
        return;
    }

    const auto message = tr("Connection error: %1").arg(socket_.errorString());
    preserveStatusOnDisconnect_ = true;
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
            const auto errorMessage =
                tr("Unsupported protocol version: %1").arg(message.protocolVersion);
            setStatusText(errorMessage);
            emit connectionError(errorMessage);
            socket_.disconnectFromHost();
            return;
        }

        const auto messageType = static_cast<MessageType>(message.messageType);
        if (!connected()) {
            if (messageType == MessageType::AuthChallenge) {
                handleAuthenticationChallenge(message);
            } else if (messageType == MessageType::AuthSuccess) {
                handleAuthenticationSuccess(message);
            } else if (messageType == MessageType::AuthFailure) {
                failAuthentication(message.text.isEmpty() ? tr("Authentication failed.")
                                                          : message.text);
            } else if (messageType == MessageType::RegistrationPending) {
                failAuthentication(
                    tr("You do not have access yet. A registration request was created and must "
                       "be approved on the server. Please try again afterwards."));
            } else if (messageType == MessageType::RegistrationRejected) {
                failAuthentication(tr("Your registration request was rejected on the server."));
            } else {
                failAuthentication(tr("Server sent an unexpected message during authentication."));
            }
            continue;
        }

        if (messageType != MessageType::ChatMessage && messageType != MessageType::SystemMessage &&
            messageType != MessageType::ErrorMessage) {
            const auto errorMessage = tr("Server sent an unexpected message.");
            setStatusText(errorMessage);
            emit connectionError(errorMessage);
            socket_.disconnectFromHost();
            return;
        }

        emit messageReceived(message.senderName, message.text,
                             message.timestamp.toLocalTime().toString(QStringLiteral("HH:mm")));
    }
}

void NetworkManager::setConnectionState(ConnectionState state) {
    if (connectionState_ == state) {
        return;
    }

    const auto wasConnected = connected();
    const auto wasBusy = busy();
    connectionState_ = state;
    if (wasConnected != connected()) {
        emit connectedChanged();
    }
    if (wasBusy != busy()) {
        emit busyChanged();
    }
}

void NetworkManager::setStatusText(const QString& statusText) {
    if (statusText_ == statusText) {
        return;
    }

    statusText_ = statusText;
    emit statusTextChanged();
}

void NetworkManager::sendAuthenticationHello() {
    if (!authenticationKeyPair_ || authenticationUserName_.isEmpty()) {
        failAuthentication(tr("No user or RSA key is available for authentication."));
        return;
    }

    clientNonce_ = messenger::protocol::generateSecureRandomBytes(
        messenger::protocol::AuthenticationNonceSize);

    Message hello;
    hello.messageType = static_cast<quint32>(MessageType::AuthHello);
    hello.senderName = authenticationUserName_;
    hello.clientNonce = clientNonce_;
    hello.publicKey = toByteArray(authenticationKeyPair_->getPublicKey().serialize());

    QDataStream out(&socket_);
    out.setVersion(DataStreamVersion);
    out << hello;
    socket_.flush();
    setStatusText(tr("Authentication request sent. Waiting for server challenge ..."));
}

void NetworkManager::handleAuthenticationChallenge(const Message& message) {
    if (connectionState_ != ConnectionState::AwaitingChallenge ||
        message.authenticationId.size() != messenger::protocol::AuthenticationIdSize ||
        message.serverNonce.size() != messenger::protocol::AuthenticationNonceSize) {
        failAuthentication(tr("The server sent an invalid authentication challenge."));
        return;
    }

    authenticationId_ = message.authenticationId;
    serverNonce_ = message.serverNonce;
    setConnectionState(ConnectionState::SigningChallenge);
    setStatusText(tr("Challenge received. Signing authentication proof ..."));

    QTimer::singleShot(0, this, [this]() {
        if (connectionState_ != ConnectionState::SigningChallenge || !authenticationKeyPair_) {
            return;
        }

        const auto transcript = messenger::protocol::authenticationTranscript(
            authenticationUserName_, authenticationId_, clientNonce_, serverNonce_);
        const auto digest = QCryptographicHash::hash(transcript, QCryptographicHash::Sha256);
        const auto signature = core::signature::signDigest(authenticationKeyPair_->getPrivateKey(),
                                                           toByteVector(digest));
        if (signature.empty()) {
            failAuthentication(tr("The RSA authentication proof could not be created."));
            return;
        }

        Message proof;
        proof.messageType = static_cast<quint32>(MessageType::AuthProof);
        proof.authenticationId = authenticationId_;
        proof.signature = toByteArray(signature);

        QDataStream out(&socket_);
        out.setVersion(DataStreamVersion);
        out << proof;
        socket_.flush();
        setConnectionState(ConnectionState::AwaitingAuthenticationResult);
        setStatusText(tr("Authentication proof sent. Waiting for verification ..."));
    });
}

void NetworkManager::handleAuthenticationSuccess(const Message& message) {
    if (connectionState_ != ConnectionState::AwaitingAuthenticationResult) {
        failAuthentication(tr("The server confirmed authentication unexpectedly."));
        return;
    }

    authenticationTimer_.stop();
    if (!message.senderName.trimmed().isEmpty() && userName_ != message.senderName.trimmed()) {
        userName_ = message.senderName.trimmed();
        emit userNameChanged();
    }
    clearAuthenticationData();
    setStatusText(tr("Authenticated as %1.").arg(userName_));
    setConnectionState(ConnectionState::Authenticated);
}

void NetworkManager::failAuthentication(const QString& message) {
    if (connected()) {
        return;
    }

    authenticationTimer_.stop();
    preserveStatusOnDisconnect_ = true;
    const auto visibleMessage = message.isEmpty() ? tr("Authentication failed.") : message;
    setStatusText(visibleMessage);
    emit connectionError(visibleMessage);
    if (socket_.state() != QAbstractSocket::UnconnectedState) {
        socket_.disconnectFromHost();
    }
}

void NetworkManager::clearAuthenticationData() {
    authenticationTimer_.stop();
    authenticationUserName_.clear();
    authenticationId_.clear();
    clientNonce_.clear();
    serverNonce_.clear();
    authenticationKeyPair_.reset();
}
