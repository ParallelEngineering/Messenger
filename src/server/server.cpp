#include "server.h"

#include <cstdio>

server& server::getInstance() {
    static server instance;
    return instance;
}

server::server() {
    std::printf("Starting Messenger Server");
}

int main() {
    auto& serverInstance = server::getInstance();
    (void)serverInstance;

    return 0;
}
