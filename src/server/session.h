#ifndef MESSENGER_SESSION_H
#define MESSENGER_SESSION_H

#include "message.h"
#include "keyPair.h"

#include <QDataStream>
#include <QDeadlineTimer>
#include <QObject>
#include <QTimer>

class QTcpSocket;

class Session final : public QObject {
    Q_OBJECT

   public:
    enum class AuthenticationState {
        AwaitingHello,
        AwaitingProof,
        Authenticated,
        Rejected,
    };

    explicit Session(QTcpSocket* socket, QObject* parent = nullptr);

    void sendMessage(const messenger::protocol::Message& message);
    void beginAuthentication(int userId,
                             const QString& userName,
                             const PublicKey& publicKey,
                             const QByteArray& authenticationId,
                             const QByteArray& clientNonce,
                             const QByteArray& serverNonce);
    void completeAuthentication();
    void rejectAuthentication(
        const QString& reason = QStringLiteral("Authentication failed."),
        messenger::protocol::MessageType messageType = messenger::protocol::MessageType::AuthFailure);
    void disconnectFromHost();

    [[nodiscard]] AuthenticationState authenticationState() const;
    [[nodiscard]] bool isAuthenticated() const;
    [[nodiscard]] bool authenticationExpired() const;
    [[nodiscard]] int userId() const;
    [[nodiscard]] QString userName() const;
    [[nodiscard]] const PublicKey& authenticationPublicKey() const;
    [[nodiscard]] const QByteArray& authenticationId() const;
    [[nodiscard]] const QByteArray& clientNonce() const;
    [[nodiscard]] const QByteArray& serverNonce() const;
    [[nodiscard]] QString peerAddress() const;

   signals:
    void messageReceived(const messenger::protocol::Message& message, Session* session);
    void disconnected(Session* session);

   private slots:
    void readAvailable();
    void handleDisconnected();
    void handleError();

   private:
    QTcpSocket* socket_;
    QDataStream stream_;
    AuthenticationState authenticationState_ = AuthenticationState::AwaitingHello;
    int userId_ = -1;
    QString userName_;
    PublicKey authenticationPublicKey_;
    QByteArray authenticationId_;
    QByteArray clientNonce_;
    QByteArray serverNonce_;
    QDeadlineTimer authenticationDeadline_;
    QTimer authenticationTimer_;
};

#endif  // MESSENGER_SESSION_H
