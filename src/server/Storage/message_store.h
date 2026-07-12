#ifndef MESSENGER_MESSAGE_STORE_H
#define MESSENGER_MESSAGE_STORE_H

#include "message.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

struct UserAuthenticationRecord {
    int userId;
    QString userName;
    QByteArray publicKey;
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
    bool saveMessage(const messenger::protocol::Message& message);
    QList<messenger::protocol::Message> loadMessages() const;

   private:
    int userIdForUserName(const QString& userName) const;
    bool openDatabase();
    bool runMigrations();
    bool ensureAdminUser(const QByteArray& publicKey) const;
    bool executeSqlScript(const QString& script) const;
    QString databasePath() const;

    QString connectionName_;
    bool initialized_ = false;
};

#endif  // MESSENGER_MESSAGE_STORE_H
