#include "message.h"

#include <QIODevice>
#include <QRandomGenerator>

namespace messenger::protocol {

QDataStream& operator<<(QDataStream& out, const Message& message) {
    out << message.protocolVersion;
    out << message.messageType;
    out << message.senderName;
    out << message.text;
    out << message.timestamp;
    out << message.authenticationId;
    out << message.clientNonce;
    out << message.serverNonce;
    out << message.signature;
    out << message.publicKey;

    return out;
}

QDataStream& operator>>(QDataStream& in, Message& message) {
    in >> message.protocolVersion;
    in >> message.messageType;
    in >> message.senderName;
    in >> message.text;
    in >> message.timestamp;
    in >> message.authenticationId;
    in >> message.clientNonce;
    in >> message.serverNonce;
    in >> message.signature;
    in >> message.publicKey;

    return in;
}

QByteArray authenticationTranscript(const QString& userName, const QByteArray& authenticationId,
                                    const QByteArray& clientNonce, const QByteArray& serverNonce) {
    QByteArray transcript;
    QDataStream stream(&transcript, QIODevice::WriteOnly);
    stream.setVersion(DataStreamVersion);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << QByteArrayLiteral("MessengerAuth/v1");
    stream << CurrentProtocolVersion;
    stream << userName.trimmed().toUtf8();
    stream << authenticationId;
    stream << clientNonce;
    stream << serverNonce;
    return transcript;
}

QByteArray generateSecureRandomBytes(qsizetype size) {
    if (size <= 0) {
        return {};
    }

    QByteArray bytes(size, Qt::Uninitialized);
    auto* generator = QRandomGenerator::system();
    for (qsizetype index = 0; index < size; ++index) {
        bytes[index] = static_cast<char>(generator->generate() & 0xFFU);
    }
    return bytes;
}

}  // namespace messenger::protocol
