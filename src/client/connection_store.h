#ifndef MESSENGER_CONNECTION_STORE_H
#define MESSENGER_CONNECTION_STORE_H

#include "keyPair.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <optional>

class ConnectionStore final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString userName READ userName WRITE setUserName NOTIFY userNameChanged)
    Q_PROPERTY(QString selectedKeyName READ selectedKeyName WRITE setSelectedKeyName NOTIFY selectedKeyNameChanged)
    Q_PROPERTY(QStringList availableKeyNames READ availableKeyNames NOTIFY availableKeyNamesChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)

   public:
    explicit ConnectionStore(QObject* parent = nullptr);

    [[nodiscard]] QString host() const;
    [[nodiscard]] int port() const;
    [[nodiscard]] QString userName() const;
    [[nodiscard]] QString selectedKeyName() const;
    [[nodiscard]] const keyPair* currentKeyPair() const;
    [[nodiscard]] QStringList availableKeyNames() const;
    [[nodiscard]] QString errorText() const;

    void setHost(const QString& host);
    void setPort(int port);
    void setUserName(const QString& userName);
    void setSelectedKeyName(const QString& selectedKeyName);

    Q_INVOKABLE bool createKeyPair(const QString& name);
    Q_INVOKABLE bool deleteKeyPair(const QString& name);
    Q_INVOKABLE void clearErrorText();
    Q_INVOKABLE bool saveLastConnection(const QString& host,
                                        int port,
                                        const QString& userName,
                                        const QString& selectedKeyName);

   signals:
    void hostChanged();
    void portChanged();
    void userNameChanged();
    void selectedKeyNameChanged();
    void availableKeyNamesChanged();
    void errorTextChanged();

   private:
    [[nodiscard]] QString appDataPath() const;
    [[nodiscard]] QString keyDirectoryPath() const;
    [[nodiscard]] QString settingsFilePath() const;
    [[nodiscard]] bool ensureKeyDirectory();
    [[nodiscard]] bool keyNameIsValid(const QString& keyName) const;
    [[nodiscard]] bool keyExists(const QString& keyName) const;
    [[nodiscard]] QString publicKeyPath(const QString& keyName) const;
    [[nodiscard]] QString privateKeyPath(const QString& keyName) const;
    [[nodiscard]] std::optional<keyPair> loadKeyPair(const QString& keyName);

    void refreshAvailableKeyNames();
    void loadLastConnection();
    void clearInvalidSelectedKey();
    void setErrorText(const QString& errorText);

    QString host_;
    int port_;
    QString userName_;
    QString selectedKeyName_;
    std::optional<keyPair> currentKeyPair_;
    QStringList availableKeyNames_;
    QString errorText_;
};

#endif  // MESSENGER_CONNECTION_STORE_H
