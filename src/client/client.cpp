#include "client.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

client& client::getInstance() {
    static client instance;
    return instance;
}

client::client() = default;

client::~client() = default;

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    auto& clientInstance = client::getInstance();
    (void)clientInstance;

    QQmlApplicationEngine engine;
    engine.loadFromModule("Messenger.Client", "Main");

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
