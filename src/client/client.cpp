#include "client.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

client& client::getInstance() {
    static client instance;
    return instance;
}

client::client() = default;

client::~client() = default;

int client::run(QGuiApplication& app) {
    engine_ = std::make_unique<QQmlApplicationEngine>();
    engine_->loadFromModule("Messenger.Client", "Main");

    if (engine_->rootObjects().isEmpty()) {
        engine_.reset();
        return -1;
    }

    const auto exitCode = app.exec();
    engine_.reset();

    return exitCode;
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    auto& clientInstance = client::getInstance();

    return clientInstance.run(app);
}
