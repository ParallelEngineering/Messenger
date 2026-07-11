#include "message_store.h"

#include "keyPair.h"

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

#include <cstdint>
#include <exception>
#include <optional>
#include <vector>

using messenger::protocol::CurrentProtocolVersion;
using messenger::protocol::Message;

namespace {

constexpr auto InvalidUserId = -1;
constexpr auto AdminPublicKeyFileName = "admin.public.rsa";

QString lastErrorText(const QSqlQuery& query) {
    return query.lastError().text();
}

std::optional<QByteArray> loadAdminPublicKey(const QString& databasePath) {
    const auto publicKeyPath = QFileInfo(databasePath).absoluteDir().filePath(
        QString::fromLatin1(AdminPublicKeyFileName));
    const QFileInfo publicKeyInfo(publicKeyPath);
    if (!publicKeyInfo.exists()) {
        qCritical() << "Admin public key file does not exist:" << publicKeyInfo.absoluteFilePath();
        return std::nullopt;
    }
    if (!publicKeyInfo.isFile()) {
        qCritical() << "Admin public key path is not a file:" << publicKeyInfo.absoluteFilePath();
        return std::nullopt;
    }

    QFile publicKeyFile(publicKeyPath);
    if (!publicKeyFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Could not read admin public key file:" << publicKeyInfo.absoluteFilePath()
                    << publicKeyFile.errorString();
        return std::nullopt;
    }

    const auto publicKeyData = publicKeyFile.readAll();
    if (publicKeyData.isEmpty()) {
        qCritical() << "Admin public key file is empty:" << publicKeyInfo.absoluteFilePath();
        return std::nullopt;
    }

    const std::vector<std::uint8_t> serializedKey(publicKeyData.cbegin(), publicKeyData.cend());
    try {
        PublicKey publicKey;
        const operations::BigInt one(1);
        if (!keyPair::s_deserialize(serializedKey, publicKey.n, publicKey.e)
            || publicKey.n <= one
            || publicKey.e <= one
            || publicKey.serialize() != serializedKey) {
            qCritical() << "Admin public key file contains an invalid RSA public key:"
                        << publicKeyInfo.absoluteFilePath();
            return std::nullopt;
        }
    } catch (const std::exception& exception) {
        qCritical() << "Could not parse admin public key file:" << publicKeyInfo.absoluteFilePath()
                    << exception.what();
        return std::nullopt;
    }

    return publicKeyData;
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

    const auto adminPublicKey = loadAdminPublicKey(databasePath());
    if (!adminPublicKey) {
        return false;
    }

    if (!openDatabase()) {
        return false;
    }

    if (!runMigrations()) {
        return false;
    }

    if (!ensureAdminUser(*adminPublicKey)) {
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
        message.messageType = query.value(QStringLiteral("message_type")).toUInt();
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

bool MessageStore::ensureAdminUser(const QByteArray& publicKey) const {
    auto database = QSqlDatabase::database(connectionName_);
    if (!database.transaction()) {
        qWarning() << "Could not start admin user transaction:" << database.lastError().text();
        return false;
    }

    QSqlQuery query(database);
    query.prepare(R"(
        INSERT INTO users (username, public_key, created_at)
        VALUES (:username, :public_key, :created_at)
        ON CONFLICT(username) DO UPDATE SET public_key = excluded.public_key
    )");
    query.bindValue(QStringLiteral(":username"), QStringLiteral("admin"));
    query.bindValue(QStringLiteral(":public_key"), publicKey);
    query.bindValue(QStringLiteral(":created_at"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    if (!query.exec()) {
        qWarning() << "Could not create or update admin user:" << lastErrorText(query);
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        qWarning() << "Could not commit admin user transaction:" << database.lastError().text();
        database.rollback();
        return false;
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
