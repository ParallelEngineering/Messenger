#include "message_store.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

#include <optional>

using messenger::protocol::CurrentProtocolVersion;
using messenger::protocol::Message;
using messenger::protocol::MessageType;

namespace {

constexpr auto InvalidUserId = -1;

QString lastErrorText(const QSqlQuery& query) {
    return query.lastError().text();
}

QList<QString> splitSqlStatements(const QString& script) {
    QList<QString> statements;
    QString current;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool inLineComment = false;

    for (qsizetype i = 0; i < script.size(); ++i) {
        const auto currentChar = script.at(i);
        const auto nextChar = i + 1 < script.size() ? script.at(i + 1) : QChar();

        if (inLineComment) {
            if (currentChar == QLatin1Char('\n')) {
                inLineComment = false;
            }
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote && currentChar == QLatin1Char('-') && nextChar == QLatin1Char('-')) {
            inLineComment = true;
            ++i;
            continue;
        }

        if (currentChar == QLatin1Char('\'') && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
        } else if (currentChar == QLatin1Char('"') && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
        }

        if (currentChar == QLatin1Char(';') && !inSingleQuote && !inDoubleQuote) {
            const auto statement = current.trimmed();
            if (!statement.isEmpty()) {
                statements.append(statement);
            }
            current.clear();
            continue;
        }

        current.append(currentChar);
    }

    const auto statement = current.trimmed();
    if (!statement.isEmpty()) {
        statements.append(statement);
    }

    return statements;
}

}  // namespace

MessageStore::MessageStore()
    : connectionName_(QStringLiteral("messenger_server_storage")) {}

MessageStore::~MessageStore() {
    if (QCoreApplication::instance() == nullptr) {
        return;
    }

    close();
}

void MessageStore::close() {
    if (!QSqlDatabase::contains(connectionName_)) {
        return;
    }

    {
        auto database = QSqlDatabase::database(connectionName_, false);
        if (database.isValid()) {
            database.close();
        }
    }

    QSqlDatabase::removeDatabase(connectionName_);
    initialized_ = false;
}

bool MessageStore::initialize() {
    if (initialized_) {
        return true;
    }

    if (!openDatabase()) {
        return false;
    }

    if (!runMigrations()) {
        return false;
    }

    initialized_ = true;
    return true;
}

bool MessageStore::saveMessage(const Message& message) {
    if (!initialized_) {
        qWarning() << "Cannot save message before message store initialization";
        return false;
    }

    const auto userId = userIdForUserName(message.senderName);
    if (userId == InvalidUserId) {
        qWarning() << "Cannot save message for unknown user:" << message.senderName;
        return false;
    }

    auto database = QSqlDatabase::database(connectionName_);
    QSqlQuery query(database);
    query.prepare(R"(
        INSERT INTO messages (
            user_id,
            message_type,
            body,
            client_timestamp,
            stored_at
        ) VALUES (
            :user_id,
            :message_type,
            :body,
            :client_timestamp,
            :stored_at
        )
    )");
    query.bindValue(QStringLiteral(":user_id"), userId);
    query.bindValue(QStringLiteral(":message_type"), message.messageType);
    query.bindValue(QStringLiteral(":body"), message.text);
    query.bindValue(QStringLiteral(":client_timestamp"),
                    message.timestamp.toUTC().toString(Qt::ISODateWithMs));
    query.bindValue(QStringLiteral(":stored_at"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    if (!query.exec()) {
        qWarning() << "Could not save chat message:" << lastErrorText(query);
        return false;
    }

    return true;
}

QList<Message> MessageStore::loadMessages() const {
    QList<Message> messages;

    if (!initialized_) {
        qWarning() << "Cannot load messages before message store initialization";
        return messages;
    }

    auto database = QSqlDatabase::database(connectionName_);
    QSqlQuery query(database);
    query.prepare(R"(
        SELECT users.username, messages.message_type, messages.body, messages.client_timestamp
        FROM messages
        INNER JOIN users ON users.id = messages.user_id
        ORDER BY messages.stored_at ASC, messages.id ASC
    )");

    if (!query.exec()) {
        qWarning() << "Could not load chat messages:" << lastErrorText(query);
        return messages;
    }

    while (query.next()) {
        Message message;
        message.protocolVersion = CurrentProtocolVersion;
        message.senderName = query.value(QStringLiteral("username")).toString();
        const auto storedMessageType = query.value(QStringLiteral("message_type")).toUInt();
        switch (storedMessageType) {
            case 1: // Protocol version 1 ChatMessage
                message.messageType = static_cast<quint32>(MessageType::ChatMessage);
                break;
            case 2: // Protocol version 1 SystemMessage
                message.messageType = static_cast<quint32>(MessageType::SystemMessage);
                break;
            case 3: // Protocol version 1 ErrorMessage
                message.messageType = static_cast<quint32>(MessageType::ErrorMessage);
                break;
            case static_cast<quint32>(MessageType::ChatMessage):
            case static_cast<quint32>(MessageType::SystemMessage):
            case static_cast<quint32>(MessageType::ErrorMessage):
                message.messageType = storedMessageType;
                break;
            default:
                qWarning() << "Skipping stored message with unsupported type"
                           << storedMessageType;
                continue;
        }
        message.text = query.value(QStringLiteral("body")).toString();
        message.timestamp = QDateTime::fromString(query.value(QStringLiteral("client_timestamp")).toString(),
                                                  Qt::ISODateWithMs);
        if (!message.timestamp.isValid()) {
            message.timestamp = QDateTime::currentDateTimeUtc();
        }
        messages.append(message);
    }

    return messages;
}

bool MessageStore::hasUser(const QString& userName) const {
    return userIdForUserName(userName) != InvalidUserId;
}

std::optional<UserAuthenticationRecord> MessageStore::findUserForAuthentication(
    const QString& userName) const {
    if (!initialized_) {
        qWarning() << "Cannot authenticate a user before message store initialization";
        return std::nullopt;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral(
        "SELECT id, username, public_key FROM users WHERE username = :username"));
    query.bindValue(QStringLiteral(":username"), userName.trimmed());

    if (!query.exec()) {
        qWarning() << "Could not load user authentication data:" << lastErrorText(query);
        return std::nullopt;
    }

    if (!query.next()) {
        return std::nullopt;
    }

    return UserAuthenticationRecord{
        query.value(QStringLiteral("id")).toInt(),
        query.value(QStringLiteral("username")).toString(),
        query.value(QStringLiteral("public_key")).toByteArray(),
    };
}

RegistrationRequestResult MessageStore::requestRegistration(
    const QString& userName,
    const QByteArray& publicKey,
    const QString& sourceAddress) const {
    if (!initialized_) {
        return {};
    }

    auto database = QSqlDatabase::database(connectionName_);
    QSqlQuery existingQuery(database);
    existingQuery.prepare(R"(
        SELECT id, status
        FROM registration_requests
        WHERE username = :username AND public_key = :public_key
    )");
    existingQuery.bindValue(QStringLiteral(":username"), userName.trimmed());
    existingQuery.bindValue(QStringLiteral(":public_key"), publicKey);
    if (!existingQuery.exec()) {
        qWarning() << "Could not look up registration request:" << lastErrorText(existingQuery);
        return {};
    }
    if (existingQuery.next()) {
        const auto status = existingQuery.value(QStringLiteral("status")).toString();
        return {
            status == QStringLiteral("rejected")
                ? RegistrationRequestResult::Status::Rejected
                : RegistrationRequestResult::Status::Pending,
            existingQuery.value(QStringLiteral("id")).toLongLong(),
        };
    }

    QSqlQuery insertQuery(database);
    insertQuery.prepare(R"(
        INSERT INTO registration_requests (
            username, public_key, source_address, status, created_at
        ) VALUES (
            :username, :public_key, :source_address, 'pending', :created_at
        )
    )");
    insertQuery.bindValue(QStringLiteral(":username"), userName.trimmed());
    insertQuery.bindValue(QStringLiteral(":public_key"), publicKey);
    insertQuery.bindValue(QStringLiteral(":source_address"), sourceAddress);
    insertQuery.bindValue(QStringLiteral(":created_at"),
                          QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!insertQuery.exec()) {
        qWarning() << "Could not create registration request:" << lastErrorText(insertQuery);
        return {};
    }

    return {RegistrationRequestResult::Status::Created, insertQuery.lastInsertId().toLongLong()};
}

QList<RegistrationRequest> MessageStore::pendingRegistrationRequests() const {
    QList<RegistrationRequest> requests;
    if (!initialized_) {
        return requests;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(R"(
        SELECT id, username, public_key, source_address, created_at
        FROM registration_requests
        WHERE status = 'pending'
        ORDER BY created_at ASC, id ASC
    )");
    if (!query.exec()) {
        qWarning() << "Could not list registration requests:" << lastErrorText(query);
        return requests;
    }

    while (query.next()) {
        requests.append({
            query.value(QStringLiteral("id")).toLongLong(),
            query.value(QStringLiteral("username")).toString(),
            query.value(QStringLiteral("public_key")).toByteArray(),
            query.value(QStringLiteral("source_address")).toString(),
            query.value(QStringLiteral("created_at")).toString(),
        });
    }
    return requests;
}

bool MessageStore::approveRegistrationRequest(qint64 requestId) const {
    if (!initialized_) {
        return false;
    }

    auto database = QSqlDatabase::database(connectionName_);
    if (!database.transaction()) {
        return false;
    }

    QSqlQuery requestQuery(database);
    requestQuery.prepare(R"(
        SELECT username, public_key
        FROM registration_requests
        WHERE id = :id AND status = 'pending'
    )");
    requestQuery.bindValue(QStringLiteral(":id"), requestId);
    if (!requestQuery.exec() || !requestQuery.next()) {
        qWarning() << "Pending registration request not found:" << requestId;
        database.rollback();
        return false;
    }

    const auto userName = requestQuery.value(QStringLiteral("username")).toString();
    const auto publicKey = requestQuery.value(QStringLiteral("public_key")).toByteArray();
    const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QSqlQuery insertUserQuery(database);
    insertUserQuery.prepare(R"(
        INSERT INTO users (username, public_key, created_at)
        VALUES (:username, :public_key, :created_at)
    )");
    insertUserQuery.bindValue(QStringLiteral(":username"), userName);
    insertUserQuery.bindValue(QStringLiteral(":public_key"), publicKey);
    insertUserQuery.bindValue(QStringLiteral(":created_at"), now);
    if (!insertUserQuery.exec()) {
        qWarning() << "Could not create user from registration request:"
                   << lastErrorText(insertUserQuery);
        database.rollback();
        return false;
    }

    QSqlQuery approveQuery(database);
    approveQuery.prepare(R"(
        UPDATE registration_requests
        SET status = 'approved', reviewed_at = :reviewed_at
        WHERE id = :id AND status = 'pending'
    )");
    approveQuery.bindValue(QStringLiteral(":reviewed_at"), now);
    approveQuery.bindValue(QStringLiteral(":id"), requestId);
    if (!approveQuery.exec() || approveQuery.numRowsAffected() != 1) {
        database.rollback();
        return false;
    }

    QSqlQuery rejectOthersQuery(database);
    rejectOthersQuery.prepare(R"(
        UPDATE registration_requests
        SET status = 'rejected', reviewed_at = :reviewed_at
        WHERE username = :username AND id <> :id AND status = 'pending'
    )");
    rejectOthersQuery.bindValue(QStringLiteral(":reviewed_at"), now);
    rejectOthersQuery.bindValue(QStringLiteral(":username"), userName);
    rejectOthersQuery.bindValue(QStringLiteral(":id"), requestId);
    if (!rejectOthersQuery.exec()) {
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        database.rollback();
        return false;
    }
    qInfo() << "Approved registration request" << requestId << "and created user" << userName;
    return true;
}

bool MessageStore::rejectRegistrationRequest(qint64 requestId) const {
    if (!initialized_) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(R"(
        UPDATE registration_requests
        SET status = 'rejected', reviewed_at = :reviewed_at
        WHERE id = :id AND status = 'pending'
    )");
    query.bindValue(QStringLiteral(":reviewed_at"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.bindValue(QStringLiteral(":id"), requestId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        qWarning() << "Pending registration request not found:" << requestId;
        return false;
    }
    qInfo() << "Rejected registration request" << requestId;
    return true;
}

int MessageStore::userIdForUserName(const QString& userName) const {
    if (!initialized_) {
        qWarning() << "Cannot look up user before message store initialization";
        return InvalidUserId;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral("SELECT id FROM users WHERE username = :username"));
    query.bindValue(QStringLiteral(":username"), userName.trimmed());

    if (!query.exec()) {
        qWarning() << "Could not look up user:" << lastErrorText(query);
        return InvalidUserId;
    }

    if (!query.next()) {
        return InvalidUserId;
    }

    return query.value(QStringLiteral("id")).toInt();
}

bool MessageStore::openDatabase() {
    if (QSqlDatabase::contains(connectionName_)) {
        return QSqlDatabase::database(connectionName_).isOpen();
    }

    auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database.setDatabaseName(databasePath());

    const auto databaseDirectory = QFileInfo(database.databaseName()).absoluteDir();
    if (!databaseDirectory.exists() && !QDir().mkpath(databaseDirectory.absolutePath())) {
        qWarning() << "Could not create database directory:" << databaseDirectory.absolutePath();
        return false;
    }

    if (!database.open()) {
        qWarning() << "Could not open SQLite database:" << database.lastError().text();
        return false;
    }

    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        qWarning() << "Could not enable SQLite foreign keys:" << lastErrorText(query);
        return false;
    }

    if (!query.exec(QStringLiteral("PRAGMA journal_mode = WAL"))) {
        qWarning() << "Could not enable SQLite WAL mode:" << lastErrorText(query);
        return false;
    }

    qInfo() << "Using SQLite database" << database.databaseName();
    return true;
}

bool MessageStore::runMigrations() {
    auto database = QSqlDatabase::database(connectionName_);
    QSqlQuery query(database);

    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version TEXT PRIMARY KEY,
            applied_at TEXT NOT NULL
        )
    )")) {
        qWarning() << "Could not create schema_migrations table:" << lastErrorText(query);
        return false;
    }

    const QDir migrationsDirectory(QCoreApplication::applicationDirPath() + QStringLiteral("/Migrations"));
    const auto migrationFiles = migrationsDirectory.entryList({QStringLiteral("*.sql")}, QDir::Files, QDir::Name);

    if (migrationFiles.isEmpty()) {
        qWarning() << "No database migrations found in" << migrationsDirectory.absolutePath();
        return false;
    }

    for (const auto& migrationFile : migrationFiles) {
        const auto version = migrationFile.section(QLatin1Char('_'), 0, 0);

        QSqlQuery appliedQuery(database);
        appliedQuery.prepare(QStringLiteral("SELECT COUNT(*) FROM schema_migrations WHERE version = :version"));
        appliedQuery.bindValue(QStringLiteral(":version"), version);
        if (!appliedQuery.exec() || !appliedQuery.next()) {
            qWarning() << "Could not check migration state for" << migrationFile << lastErrorText(appliedQuery);
            return false;
        }

        if (appliedQuery.value(0).toInt() > 0) {
            continue;
        }

        QFile file(migrationsDirectory.filePath(migrationFile));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Could not open migration" << file.fileName() << file.errorString();
            return false;
        }

        const auto script = QString::fromUtf8(file.readAll());
        if (!database.transaction()) {
            qWarning() << "Could not start migration transaction:" << database.lastError().text();
            return false;
        }

        if (!executeSqlScript(script)) {
            database.rollback();
            return false;
        }

        QSqlQuery insertQuery(database);
        insertQuery.prepare(R"(
            INSERT INTO schema_migrations (version, applied_at)
            VALUES (:version, :applied_at)
        )");
        insertQuery.bindValue(QStringLiteral(":version"), version);
        insertQuery.bindValue(QStringLiteral(":applied_at"),
                              QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

        if (!insertQuery.exec()) {
            qWarning() << "Could not record migration" << migrationFile << lastErrorText(insertQuery);
            database.rollback();
            return false;
        }

        if (!database.commit()) {
            qWarning() << "Could not commit migration" << migrationFile << database.lastError().text();
            database.rollback();
            return false;
        }

        qInfo() << "Applied database migration" << migrationFile;
    }

    return true;
}

bool MessageStore::executeSqlScript(const QString& script) const {
    auto database = QSqlDatabase::database(connectionName_);

    for (const auto& statement : splitSqlStatements(script)) {
        QSqlQuery query(database);
        if (!query.exec(statement)) {
            qWarning() << "Could not execute SQL migration statement:" << lastErrorText(query);
            qWarning() << statement;
            return false;
        }
    }

    return true;
}

QString MessageStore::databasePath() const {
    const auto configuredPath = qEnvironmentVariable("MESSENGER_DB_PATH");
    if (!configuredPath.isEmpty()) {
        return configuredPath;
    }

    auto appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty()) {
        appDataPath = QDir::homePath() + QStringLiteral("/.messenger");
    }

    return QDir(appDataPath).filePath(QStringLiteral("messenger.sqlite3"));
}
