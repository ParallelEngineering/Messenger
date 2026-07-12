#include "connection_store.h"

#include "keyPair.h"
#include "message.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QMetaObject>
#include <QPointer>
#include <QSaveFile>
#include <QStandardPaths>

#include <exception>
#include <utility>
#include <vector>

namespace {

constexpr auto KeyFolderName = "rsa-keys";
constexpr auto SettingsFileName = "client-connection.json";
constexpr auto PublicKeySuffix = ".public.rsa";
constexpr auto PrivateKeySuffix = ".private.rsa";

QByteArray toByteArray(const std::vector<uint8_t>& bytes) {
    return QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()));
}

std::vector<uint8_t> toByteVector(const QByteArray& bytes) {
    const auto* begin = reinterpret_cast<const uint8_t*>(bytes.constData());
    return {begin, begin + bytes.size()};
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

ConnectionStore::~ConnectionStore() {
    cancelKeyPairCreation();
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

const keyPair* ConnectionStore::currentKeyPair() const {
    return currentKeyPair_ ? &*currentKeyPair_ : nullptr;
}

QStringList ConnectionStore::availableKeyNames() const {
    return availableKeyNames_;
}

QString ConnectionStore::errorText() const {
    return errorText_;
}

bool ConnectionStore::keyGenerationInProgress() const { return keyGenerationInProgress_; }

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

    if (trimmedKeyName.isEmpty()) {
        const auto selectionChanged = !selectedKeyName_.isEmpty();
        selectedKeyName_.clear();
        currentKeyPair_.reset();
        if (selectionChanged) {
            emit selectedKeyNameChanged();
        }
        return;
    }

    if (selectedKeyName_ == trimmedKeyName && currentKeyPair_) {
        return;
    }

    auto loadedKeyPair = loadKeyPair(trimmedKeyName);
    if (!loadedKeyPair) {
        // Re-emit the current value so QML controls return to the still-active selection.
        emit selectedKeyNameChanged();
        return;
    }

    const auto selectionChanged = selectedKeyName_ != trimmedKeyName;
    selectedKeyName_ = trimmedKeyName;
    currentKeyPair_ = std::move(*loadedKeyPair);
    setErrorText({});
    if (selectionChanged) {
        emit selectedKeyNameChanged();
    }
}

bool ConnectionStore::startKeyPairCreation(const QString& name) {
    const auto keyName = name.trimmed();
    setErrorText({});

    if (!keyNameIsValid(keyName)) {
        setErrorText(tr("The key name is invalid."));
        return false;
    }

    if (!ensureKeyDirectory()) {
        setErrorText(tr("The key directory could not be created."));
        return false;
    }

    if (keyExists(keyName)) {
        setErrorText(tr("A key with this name already exists."));
        return false;
    }

    if (discardGeneratedKey_) {
        discardGeneratedKey_->store(true, std::memory_order_relaxed);
    }
    discardGeneratedKey_ = std::make_shared<std::atomic_bool>(false);
    keyGenerationInProgress_ = true;
    emit keyGenerationInProgressChanged();

    const QPointer<ConnectionStore> guardedThis(this);
    const auto discardGeneratedKey = discardGeneratedKey_;
    keyGenerationThreads_.emplace_back([guardedThis, discardGeneratedKey, keyName]() {
        QByteArray publicBytes;
        QByteArray privateBytes;
        QString error;
        try {
            keyPair generated;
            if (!discardGeneratedKey->load(std::memory_order_relaxed)) {
                publicBytes = toByteArray(generated.getPublicKey().serialize());
                privateBytes = toByteArray(generated.getPrivateKey().serialize());
            }
        } catch (const std::exception& exception) {
            error = QString::fromUtf8(exception.what());
        }
        if (!guardedThis) return;
        const auto discarded = discardGeneratedKey->load(std::memory_order_relaxed);
        QMetaObject::invokeMethod(guardedThis, [guardedThis, keyName, publicBytes, privateBytes, error,
                                                discarded, discardGeneratedKey]() {
            if (guardedThis) {
                guardedThis->finishKeyPairCreation(keyName, publicBytes, privateBytes, error,
                                                   discarded, discardGeneratedKey);
            }
        }, Qt::QueuedConnection);
    });
    return true;
}

void ConnectionStore::cancelKeyPairCreation() {
    if (!keyGenerationInProgress_) return;
    discardGeneratedKey_->store(true, std::memory_order_relaxed);
    keyGenerationInProgress_ = false;
    emit keyGenerationInProgressChanged();
}

void ConnectionStore::finishKeyPairCreation(const QString& keyName, const QByteArray& publicKeyBytes,
                                            const QByteArray& privateKeyBytes, const QString& workerError,
                                            bool discarded,
                                            const std::shared_ptr<std::atomic_bool>& discardGeneratedKey) {
    discarded = discarded || discardGeneratedKey->load(std::memory_order_relaxed);

    if (!discarded && workerError.isEmpty()) {
        if (keyExists(keyName)) {
            setErrorText(tr("A key with this name already exists."));
        } else {
            QFile publicKeyFile(publicKeyPath(keyName));
            if (!publicKeyFile.open(QIODevice::WriteOnly | QIODevice::NewOnly) ||
                publicKeyFile.write(publicKeyBytes) != publicKeyBytes.size()) {
                publicKeyFile.remove();
                setErrorText(tr("The public key could not be saved."));
            } else {
                publicKeyFile.close();
                QFile privateKeyFile(privateKeyPath(keyName));
                if (!privateKeyFile.open(QIODevice::WriteOnly | QIODevice::NewOnly) ||
                    privateKeyFile.write(privateKeyBytes) != privateKeyBytes.size()) {
                    privateKeyFile.remove();
                    QFile::remove(publicKeyPath(keyName));
                    setErrorText(tr("The private key could not be saved."));
                } else {
                    privateKeyFile.close();
                    refreshAvailableKeyNames();
                    setSelectedKeyName(keyName);
                }
            }
        }
    } else if (!discarded) {
        setErrorText(tr("The key pair could not be created: %1").arg(workerError));
    }

    if (discardGeneratedKey_ == discardGeneratedKey) {
        discardGeneratedKey_.reset();
        keyGenerationInProgress_ = false;
        emit keyGenerationInProgressChanged();
    }
}

bool ConnectionStore::deleteKeyPair(const QString& name) {
    const auto keyName = name.trimmed();
    setErrorText({});

    if (keyGenerationInProgress_) {
        setErrorText(tr("Keys cannot be deleted while a key is being generated."));
        return false;
    }

    if (!availableKeyNames_.contains(keyName)) {
        setErrorText(tr("The key was not found."));
        return false;
    }

    const auto publicRemoved = QFile::remove(publicKeyPath(keyName));
    const auto privateRemoved = QFile::remove(privateKeyPath(keyName));
    if (!publicRemoved || !privateRemoved) {
        setErrorText(tr("The key could not be deleted completely."));
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
        setErrorText(tr("The connection details are incomplete."));
        return false;
    }

    if (selectedKeyName_ != trimmedKeyName || !currentKeyPair_) {
        setSelectedKeyName(trimmedKeyName);
        if (selectedKeyName_ != trimmedKeyName || !currentKeyPair_) {
            return false;
        }
    }

    const auto settingsFileInfo = QFileInfo(settingsFilePath());
    if (!settingsFileInfo.absoluteDir().exists() && !QDir().mkpath(settingsFileInfo.absolutePath())) {
        setErrorText(tr("The storage directory could not be created."));
        return false;
    }

    QJsonObject root;
    root.insert(QStringLiteral("host"), trimmedHost);
    root.insert(QStringLiteral("port"), port);
    root.insert(QStringLiteral("userName"), trimmedUserName);
    root.insert(QStringLiteral("selectedKeyName"), trimmedKeyName);

    QSaveFile settingsFile(settingsFilePath());
    if (!settingsFile.open(QIODevice::WriteOnly)) {
        setErrorText(tr("The most recent connection details could not be saved."));
        return false;
    }

    settingsFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!settingsFile.commit()) {
        setErrorText(tr("The most recent connection details could not be written."));
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

std::optional<keyPair> ConnectionStore::loadKeyPair(const QString& keyName) {
    QFile publicKeyFile(publicKeyPath(keyName));
    if (!publicKeyFile.open(QIODevice::ReadOnly)) {
        setErrorText(tr("The public key could not be read."));
        return std::nullopt;
    }
    const auto publicKeyBytes = toByteVector(publicKeyFile.readAll());

    QFile privateKeyFile(privateKeyPath(keyName));
    if (!privateKeyFile.open(QIODevice::ReadOnly)) {
        setErrorText(tr("The private key could not be read."));
        return std::nullopt;
    }
    const auto privateKeyBytes = toByteVector(privateKeyFile.readAll());

    try {
        return keyPair::create(publicKeyBytes, privateKeyBytes);
    } catch (const std::exception&) {
        setErrorText(tr("The key pair could not be deserialized."));
        return std::nullopt;
    }
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
    if (selectedKeyName_.isEmpty()) {
        currentKeyPair_.reset();
        return;
    }

    if (availableKeyNames_.contains(selectedKeyName_) && currentKeyPair_) {
        return;
    }

    setSelectedKeyName({});
}

void ConnectionStore::setErrorText(const QString& errorText) {
    if (errorText_ == errorText) {
        return;
    }

    errorText_ = errorText;
    emit errorTextChanged();
}
