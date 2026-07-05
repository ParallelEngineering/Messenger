#ifndef MESSENGER_CLIENT_NETWORK_MANAGER_H
#define MESSENGER_CLIENT_NETWORK_MANAGER_H

#include "message_envelope.h"

#include <QDataStream>
#include <QObject>
#include <QTcpSocket>

class ClientNetworkManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(int defaultPort READ defaultPort CONSTANT)

   public:
    explicit ClientNetworkManager(QObject* parent = nullptr);

    [[nodiscard]] bool connected() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] int defaultPort() const;

    Q_INVOKABLE void connectToServer(const QString& host, quint16 port);
    Q_INVOKABLE void disconnectFromServer();
    Q_INVOKABLE void sendChatMessage(const QString& sender, const QString& recipient, const QString& text);

   signals:
    void connectedChanged();
    void statusTextChanged();
    void messageReceived(const QString& sender, const QString& recipient, const QString& text);
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
};

#endif  // MESSENGER_CLIENT_NETWORK_MANAGER_H
