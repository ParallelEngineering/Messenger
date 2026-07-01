#include "server.h"

#include <iostream>

server& server::getInstance() {
    static server instance;
    return instance;
}

server::server() { std::cout << "Starting Messenger Server ...\n"; }

server::~server() { std::cout << "Stopping Messenger Server ...\n"; }

int main() {
    auto& serverInstance = server::getInstance();
    (void)serverInstance;

    return 0;
}
