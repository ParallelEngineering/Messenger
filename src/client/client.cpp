#include "client.h"

client& client::getInstance() {
    static client instance;
    return instance;
}

client::client() = default;

client::~client() = default;

int main() {
    auto& clientInstance = client::getInstance();
    (void)clientInstance;

    return 0;
}
