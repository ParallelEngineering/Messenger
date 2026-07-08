#include "message.h"

namespace messenger::protocol {

QDataStream& operator<<(QDataStream& out, const Message& message) {
    out << message.protocolVersion;
    out << message.messageType;
    out << message.senderName;
    out << message.text;
    out << message.timestamp;

    return out;
}

QDataStream& operator>>(QDataStream& in, Message& message) {
    in >> message.protocolVersion;
    in >> message.messageType;
    in >> message.senderName;
    in >> message.text;
    in >> message.timestamp;

    return in;
}

}  // namespace messenger::protocol
