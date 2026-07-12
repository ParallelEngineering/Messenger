#include "client.h"

#include "connection_store.h"
#include "network_manager.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

client& client::getInstance() {
    static client instance;
    return instance;
}

client::client() = default;

client::~client() = default;

int client::run(QGuiApplication& app) {
    QCoreApplication::setOrganizationName(QStringLiteral("ParallelEngineering"));
    QCoreApplication::setApplicationName(QStringLiteral("Messenger"));

    connectionStore_ = std::make_unique<ConnectionStore>();
    networkManager_ = std::make_unique<NetworkManager>(connectionStore_.get());
    networkManager_->setUserName(connectionStore_->userName());

    engine_ = std::make_unique<QQmlApplicationEngine>();
    engine_->rootContext()->setContextProperty("networkManager", networkManager_.get());
    engine_->rootContext()->setContextProperty("connectionStore", connectionStore_.get());
    engine_->loadFromModule("Messenger.Client", "Main");

    if (engine_->rootObjects().isEmpty()) {
        engine_.reset();
        networkManager_.reset();
        connectionStore_.reset();
        return -1;
    }

    const auto exitCode = app.exec();
    engine_.reset();
    networkManager_.reset();
    connectionStore_.reset();

    return exitCode;
}

int main(int argc, char* argv[]) {
    QQuickStyle::setStyle(QStringLiteral("Material"));
    QGuiApplication app(argc, argv);

    auto& clientInstance = client::getInstance();

    return clientInstance.run(app);
}
