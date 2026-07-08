#ifndef MESSENGER_NETWORK_MANAGER_H
#define MESSENGER_NETWORK_MANAGER_H

#include "message.h"

#include <QDataStream>
#include <QObject>
#include <QTcpSocket>

class NetworkManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString userName READ userName WRITE setUserName NOTIFY userNameChanged)
    Q_PROPERTY(int defaultPort READ defaultPort CONSTANT)

   public:
    explicit NetworkManager(QObject* parent = nullptr);

    [[nodiscard]] bool connected() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString userName() const;
    [[nodiscard]] int defaultPort() const;
    void setUserName(const QString& userName);

    // Methode that can be called from QML
    Q_INVOKABLE void connectToServer(const QString& host, quint16 port);
    Q_INVOKABLE void disconnectFromServer();
    Q_INVOKABLE void sendChatMessage(const QString& text);

   signals:
    // These methods are implemented in qt
    void connectedChanged();
    void statusTextChanged();
    void userNameChanged();
    void messageReceived(const QString& senderName, const QString& text, const QString& sentAt);
    void connectionError(const QString& message);

   private slots:
    void handleConnected();
    void handleDisconnected();
    void handleError();
    void readAvailable();

   private:
    void setStatusText(const QString& statusText);
    void emitConnectedIfChanged(bool previousConnected);

    QTcpSocket socket_;
    QDataStream stream_;
    QString statusText_;
    QString userName_;
};

#endif  // MESSENGER_NETWORK_MANAGER_H
