#include "connection_store.h"

#include "keyPair.h"
#include "message.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

#include <vector>

namespace {

constexpr auto KeyFolderName = "rsa-keys";
constexpr auto SettingsFileName = "client-connection.json";
constexpr auto PublicKeySuffix = ".public.rsa";
constexpr auto PrivateKeySuffix = ".private.rsa";

QByteArray toByteArray(const std::vector<uint8_t>& bytes) {
    return QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()));
}

}  // namespace

ConnectionStore::ConnectionStore(QObject* parent)
    : QObject(parent),
      host_(QStringLiteral("127.0.0.1")),
      port_(messenger::protocol::DefaultPort),
      userName_(QStringLiteral("anonymous")) {
    refreshAvailableKeyNames();
    loadLastConnection();
    clearInvalidSelectedKey();
}

QString ConnectionStore::host() const {
    return host_;
}

int ConnectionStore::port() const {
    return port_;
}

QString ConnectionStore::userName() const {
    return userName_;
}

QString ConnectionStore::selectedKeyName() const {
    return selectedKeyName_;
}

QStringList ConnectionStore::availableKeyNames() const {
    return availableKeyNames_;
}

QString ConnectionStore::errorText() const {
    return errorText_;
}

void ConnectionStore::setHost(const QString& host) {
    const auto trimmedHost = host.trimmed();
    if (host_ == trimmedHost) {
        return;
    }

    host_ = trimmedHost;
    emit hostChanged();
}

void ConnectionStore::setPort(int port) {
    if (port_ == port) {
        return;
    }

    port_ = port;
    emit portChanged();
}

void ConnectionStore::setUserName(const QString& userName) {
    const auto trimmedUserName = userName.trimmed();
    if (userName_ == trimmedUserName) {
        return;
    }

    userName_ = trimmedUserName;
    emit userNameChanged();
}

void ConnectionStore::setSelectedKeyName(const QString& selectedKeyName) {
    const auto trimmedKeyName = selectedKeyName.trimmed();
    if (selectedKeyName_ == trimmedKeyName) {
        return;
    }

    selectedKeyName_ = trimmedKeyName;
    emit selectedKeyNameChanged();
}

bool ConnectionStore::createKeyPair(const QString& name) {
    const auto keyName = name.trimmed();
    setErrorText({});

    if (!keyNameIsValid(keyName)) {
        setErrorText(tr("Der Schlüsselname ist ungültig."));
        return false;
    }

    if (!ensureKeyDirectory()) {
        setErrorText(tr("Der Schlüsselordner konnte nicht erstellt werden."));
        return false;
    }

    if (keyExists(keyName)) {
        setErrorText(tr("Ein Schlüssel mit diesem Namen existiert bereits."));
        return false;
    }

    keyPair generatedKeyPair;
    const auto publicKeyBytes = toByteArray(generatedKeyPair.getPublicKey().serialize());
    const auto privateKeyBytes = toByteArray(generatedKeyPair.getPrivateKey().serialize());

    QFile publicKeyFile(publicKeyPath(keyName));
    if (!publicKeyFile.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        setErrorText(tr("Der öffentliche Schlüssel konnte nicht gespeichert werden."));
        return false;
    }

    if (publicKeyFile.write(publicKeyBytes) != publicKeyBytes.size()) {
        publicKeyFile.remove();
        setErrorText(tr("Der öffentliche Schlüssel konnte nicht vollständig gespeichert werden."));
        return false;
    }
    publicKeyFile.close();

    QFile privateKeyFile(privateKeyPath(keyName));
    if (!privateKeyFile.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        QFile::remove(publicKeyPath(keyName));
        setErrorText(tr("Der private Schlüssel konnte nicht gespeichert werden."));
        return false;
    }

    if (privateKeyFile.write(privateKeyBytes) != privateKeyBytes.size()) {
        privateKeyFile.remove();
        QFile::remove(publicKeyPath(keyName));
        setErrorText(tr("Der private Schlüssel konnte nicht vollständig gespeichert werden."));
        return false;
    }
    privateKeyFile.close();

    refreshAvailableKeyNames();
    setSelectedKeyName(keyName);
    return true;
}

bool ConnectionStore::deleteKeyPair(const QString& name) {
    const auto keyName = name.trimmed();
    setErrorText({});

    if (!availableKeyNames_.contains(keyName)) {
        setErrorText(tr("Der Schlüssel wurde nicht gefunden."));
        return false;
    }

    const auto publicRemoved = QFile::remove(publicKeyPath(keyName));
    const auto privateRemoved = QFile::remove(privateKeyPath(keyName));
    if (!publicRemoved || !privateRemoved) {
        setErrorText(tr("Der Schlüssel konnte nicht vollständig gelöscht werden."));
        refreshAvailableKeyNames();
        clearInvalidSelectedKey();
        return false;
    }

    refreshAvailableKeyNames();
    if (selectedKeyName_ == keyName) {
        setSelectedKeyName(availableKeyNames_.isEmpty() ? QString() : availableKeyNames_.first());
    }

    return true;
}

void ConnectionStore::clearErrorText() {
    setErrorText({});
}

bool ConnectionStore::saveLastConnection(const QString& host,
                                               int port,
                                               const QString& userName,
                                               const QString& selectedKeyName) {
    const auto trimmedHost = host.trimmed();
    const auto trimmedUserName = userName.trimmed();
    const auto trimmedKeyName = selectedKeyName.trimmed();
    setErrorText({});

    if (trimmedHost.isEmpty() || trimmedUserName.isEmpty() || port < 1 || port > 65535 ||
        !availableKeyNames_.contains(trimmedKeyName)) {
        setErrorText(tr("Die Verbindungsdaten sind unvollständig."));
        return false;
    }

    const auto settingsFileInfo = QFileInfo(settingsFilePath());
    if (!settingsFileInfo.absoluteDir().exists() && !QDir().mkpath(settingsFileInfo.absolutePath())) {
        setErrorText(tr("Der Speicherordner konnte nicht erstellt werden."));
        return false;
    }

    QJsonObject root;
    root.insert(QStringLiteral("host"), trimmedHost);
    root.insert(QStringLiteral("port"), port);
    root.insert(QStringLiteral("userName"), trimmedUserName);
    root.insert(QStringLiteral("selectedKeyName"), trimmedKeyName);

    QSaveFile settingsFile(settingsFilePath());
    if (!settingsFile.open(QIODevice::WriteOnly)) {
        setErrorText(tr("Die letzten Verbindungsdaten konnten nicht gespeichert werden."));
        return false;
    }

    settingsFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!settingsFile.commit()) {
        setErrorText(tr("Die letzten Verbindungsdaten konnten nicht geschrieben werden."));
        return false;
    }

    setHost(trimmedHost);
    setPort(port);
    setUserName(trimmedUserName);
    setSelectedKeyName(trimmedKeyName);
    return true;
}

QString ConnectionStore::appDataPath() const {
    auto path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (path.isEmpty()) {
        path = QDir::homePath() + QStringLiteral("/.messenger");
    }

    return path;
}

QString ConnectionStore::keyDirectoryPath() const {
    return QDir(appDataPath()).filePath(QString::fromLatin1(KeyFolderName));
}

QString ConnectionStore::settingsFilePath() const {
    return QDir(appDataPath()).filePath(QString::fromLatin1(SettingsFileName));
}

bool ConnectionStore::ensureKeyDirectory() {
    const QDir keyDirectory(keyDirectoryPath());
    if (keyDirectory.exists()) {
        return true;
    }

    return QDir().mkpath(keyDirectory.absolutePath());
}

bool ConnectionStore::keyNameIsValid(const QString& keyName) const {
    static const QRegularExpression invalidCharacters(QStringLiteral(R"([\\/:*?"<>|\x00-\x1F])"));

    return !keyName.isEmpty() && keyName != QStringLiteral(".") && keyName != QStringLiteral("..") &&
           !keyName.contains(invalidCharacters);
}

bool ConnectionStore::keyExists(const QString& keyName) const {
    return QFileInfo::exists(publicKeyPath(keyName)) || QFileInfo::exists(privateKeyPath(keyName));
}

QString ConnectionStore::publicKeyPath(const QString& keyName) const {
    return QDir(keyDirectoryPath()).filePath(keyName + QString::fromLatin1(PublicKeySuffix));
}

QString ConnectionStore::privateKeyPath(const QString& keyName) const {
    return QDir(keyDirectoryPath()).filePath(keyName + QString::fromLatin1(PrivateKeySuffix));
}

void ConnectionStore::refreshAvailableKeyNames() {
    const QDir keyDirectory(keyDirectoryPath());
    const auto publicKeyFiles = keyDirectory.entryList({QStringLiteral("*") + QString::fromLatin1(PublicKeySuffix)},
                                                       QDir::Files,
                                                       QDir::Name | QDir::IgnoreCase);

    QStringList keyNames;
    for (const auto& publicKeyFile : publicKeyFiles) {
        auto keyName = publicKeyFile;
        keyName.chop(QString::fromLatin1(PublicKeySuffix).size());
        if (QFileInfo::exists(privateKeyPath(keyName))) {
            keyNames.append(keyName);
        }
    }

    if (availableKeyNames_ == keyNames) {
        return;
    }

    availableKeyNames_ = keyNames;
    emit availableKeyNamesChanged();
}

void ConnectionStore::loadLastConnection() {
    QFile settingsFile(settingsFilePath());
    if (!settingsFile.open(QIODevice::ReadOnly)) {
        return;
    }

    const auto document = QJsonDocument::fromJson(settingsFile.readAll());
    if (!document.isObject()) {
        return;
    }

    const auto root = document.object();
    if (root.value(QStringLiteral("host")).isString()) {
        setHost(root.value(QStringLiteral("host")).toString());
    }

    if (root.value(QStringLiteral("port")).isDouble()) {
        const auto loadedPort = root.value(QStringLiteral("port")).toInt();
        if (loadedPort >= 1 && loadedPort <= 65535) {
            setPort(loadedPort);
        }
    }

    if (root.value(QStringLiteral("userName")).isString()) {
        setUserName(root.value(QStringLiteral("userName")).toString());
    }

    if (root.value(QStringLiteral("selectedKeyName")).isString()) {
        setSelectedKeyName(root.value(QStringLiteral("selectedKeyName")).toString());
    }
}

void ConnectionStore::clearInvalidSelectedKey() {
    if (selectedKeyName_.isEmpty() || availableKeyNames_.contains(selectedKeyName_)) {
        return;
    }

    selectedKeyName_.clear();
    emit selectedKeyNameChanged();
}

void ConnectionStore::setErrorText(const QString& errorText) {
    if (errorText_ == errorText) {
        return;
    }

    errorText_ = errorText;
    emit errorTextChanged();
}
