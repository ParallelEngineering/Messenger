#ifndef MESSENGER_MESSAGE_H
#define MESSENGER_MESSAGE_H

#include <QDataStream>
#include <QDateTime>
#include <QString>
#include <QtGlobal>

namespace messenger::protocol {

inline constexpr quint32 CurrentProtocolVersion = 1;
inline constexpr quint16 DefaultPort = 4242;
inline constexpr qint64 ReadBufferSize = 1024 * 1024;
inline constexpr auto DataStreamVersion = QDataStream::Qt_6_5;

enum class MessageType : quint32 {
    ChatMessage = 1,
    SystemMessage = 2,
    ErrorMessage = 3,
};

struct Message {
    quint32 protocolVersion = CurrentProtocolVersion;
    quint32 messageType = static_cast<quint32>(MessageType::ChatMessage);
    QString text;
    QDateTime timestamp = QDateTime::currentDateTimeUtc();
};

QDataStream& operator<<(QDataStream& out, const Message& message);
QDataStream& operator>>(QDataStream& in, Message& message);

}  // namespace messenger::protocol

#endif  // MESSENGER_MESSAGE_H
