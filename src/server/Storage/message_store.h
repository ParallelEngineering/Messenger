#ifndef MESSENGER_MESSAGE_STORE_H
#define MESSENGER_MESSAGE_STORE_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <optional>

#include "message.h"

struct UserAuthenticationRecord {
    int userId;
    QString userName;
    QByteArray publicKey;
};

struct StoredUser {
    qint64 userId;
    QString userName;
    QString createdAt;
};

struct RegistrationRequest {
    qint64 requestId;
    QString userName;
    QByteArray publicKey;
    QString sourceAddress;
    QString createdAt;
};

struct RegistrationRequestResult {
    enum class Status {
        Created,
        Pending,
        Rejected,
        Failed,
    };

    Status status = Status::Failed;
    qint64 requestId = -1;
};

class MessageStore {
   public:
    MessageStore();
    ~MessageStore();

    MessageStore(const MessageStore&) = delete;
    MessageStore& operator=(const MessageStore&) = delete;
    MessageStore(MessageStore&&) = delete;
    MessageStore& operator=(MessageStore&&) = delete;

    bool initialize();
    void close();
    bool hasUser(const QString& userName) const;
    [[nodiscard]] std::optional<UserAuthenticationRecord> findUserForAuthentication(
        const QString& userName) const;
    [[nodiscard]] RegistrationRequestResult requestRegistration(const QString& userName,
                                                                const QByteArray& publicKey,
                                                                const QString& sourceAddress) const;
    [[nodiscard]] QList<RegistrationRequest> pendingRegistrationRequests() const;
    bool approveRegistrationRequest(qint64 requestId) const;
    bool rejectRegistrationRequest(qint64 requestId) const;
    [[nodiscard]] QList<StoredUser> users() const;
    bool deleteUser(qint64 userId) const;
    bool clearMessages() const;
    bool saveMessage(const messenger::protocol::Message& message);
    QList<messenger::protocol::Message> loadMessages() const;

   private:
    int userIdForUserName(const QString& userName) const;
    bool openDatabase();
    bool runMigrations();
    bool executeSqlScript(const QString& script) const;
    QString databasePath() const;

    QString connectionName_;
    bool initialized_ = false;
};

#endif  // MESSENGER_MESSAGE_STORE_H
