#ifndef MESSENGER_CLIENT_H
#define MESSENGER_CLIENT_H

#include <memory>

class QGuiApplication;
class QQmlApplicationEngine;

class client {
   public:
    static client& getInstance();
    ~client();

    int run(QGuiApplication& app);

    client(const client&) = delete;
    client& operator=(const client&) = delete;
    client(client&&) = delete;
    client& operator=(client&&) = delete;

   private:
    client();

    std::unique_ptr<QQmlApplicationEngine> engine_;
};

#endif  // MESSENGER_CLIENT_H
