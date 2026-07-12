#ifndef MESSENGER_NETWORK_MANAGER_H
#define MESSENGER_NETWORK_MANAGER_H

#include "message.h"

#include <QDataStream>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

#include <optional>

#include "keyPair.h"

class ConnectionStore;

class NetworkManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString userName READ userName WRITE setUserName NOTIFY userNameChanged)
    Q_PROPERTY(int defaultPort READ defaultPort CONSTANT)

   public:
    explicit NetworkManager(ConnectionStore* connectionStore, QObject* parent = nullptr);

    [[nodiscard]] bool connected() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString userName() const;
    [[nodiscard]] int defaultPort() const;
    void setUserName(const QString& userName);

    // Methode that can be called from QML
    Q_INVOKABLE void connectToServer(const QString& host, quint16 port);
    Q_INVOKABLE void disconnectFromServer();
    Q_INVOKABLE void sendChatMessage(const QString& text);

   signals:
    // These methods are implemented in qt
    void connectedChanged();
    void busyChanged();
    void statusTextChanged();
    void userNameChanged();
    void messageReceived(const QString& senderName, const QString& text, const QString& sentAt);
    void connectionError(const QString& message);

   private slots:
    void handleConnected();
    void handleDisconnected();
    void handleError();
    void readAvailable();

   private:
    enum class ConnectionState {
        Disconnected,
        Connecting,
        AwaitingChallenge,
        SigningChallenge,
        AwaitingAuthenticationResult,
        Authenticated,
    };

    void setConnectionState(ConnectionState state);
    void setStatusText(const QString& statusText);
    void sendAuthenticationHello();
    void handleAuthenticationChallenge(const messenger::protocol::Message& message);
    void handleAuthenticationSuccess(const messenger::protocol::Message& message);
    void failAuthentication(const QString& message);
    void clearAuthenticationData();

    ConnectionStore* connectionStore_;
    QTcpSocket socket_;
    QDataStream stream_;
    QTimer authenticationTimer_;
    ConnectionState connectionState_ = ConnectionState::Disconnected;
    QString statusText_;
    QString userName_;
    QString authenticationUserName_;
    QByteArray authenticationId_;
    QByteArray clientNonce_;
    QByteArray serverNonce_;
    std::optional<keyPair> authenticationKeyPair_;
    bool preserveStatusOnDisconnect_ = false;
    bool userRequestedDisconnect_ = false;
};

#endif  // MESSENGER_NETWORK_MANAGER_H
