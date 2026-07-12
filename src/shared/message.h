#ifndef MESSENGER_MESSAGE_H
#define MESSENGER_MESSAGE_H

#include <QDataStream>
#include <QDateTime>
#include <QByteArray>
#include <QString>
#include <QtGlobal>

namespace messenger::protocol {

inline constexpr quint32 CurrentProtocolVersion = 3;
inline constexpr quint16 DefaultPort = 4242;
inline constexpr qint64 ReadBufferSize = 1024 * 1024;
inline constexpr auto DataStreamVersion = QDataStream::Qt_6_5;
inline constexpr qsizetype AuthenticationIdSize = 16;
inline constexpr qsizetype AuthenticationNonceSize = 32;
inline constexpr int AuthenticationTimeoutMs = 30'000;
inline constexpr qsizetype MaximumUserNameSize = 64;
inline constexpr qsizetype MaximumMessageSize = 16 * 1024;

enum class MessageType : quint32 {
    AuthHello = 1,
    AuthChallenge = 2,
    AuthProof = 3,
    AuthSuccess = 4,
    AuthFailure = 5,
    RegistrationPending = 6,
    RegistrationRejected = 7,
    ChatMessage = 100,
    SystemMessage = 101,
    ErrorMessage = 102,
};

struct Message {
    quint32 protocolVersion = CurrentProtocolVersion;
    quint32 messageType = static_cast<quint32>(MessageType::ChatMessage);
    QString senderName;
    QString text;
    QDateTime timestamp = QDateTime::currentDateTimeUtc();
    QByteArray authenticationId;
    QByteArray clientNonce;
    QByteArray serverNonce;
    QByteArray signature;
    QByteArray publicKey;
};

QDataStream& operator<<(QDataStream& out, const Message& message);
QDataStream& operator>>(QDataStream& in, Message& message);

[[nodiscard]] QByteArray authenticationTranscript(
    const QString& userName,
    const QByteArray& authenticationId,
    const QByteArray& clientNonce,
    const QByteArray& serverNonce);

[[nodiscard]] QByteArray generateSecureRandomBytes(qsizetype size);

}  // namespace messenger::protocol

#endif  // MESSENGER_MESSAGE_H
