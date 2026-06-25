#ifndef MESSENGER_SERVER_H
#define MESSENGER_SERVER_H



class server {
public:
    static server& getInstance();

    server(const server&) = delete;
    server& operator=(const server&) = delete;
    server(server&&) = delete;
    server& operator=(server&&) = delete;

private:
    server();
};



#endif //MESSENGER_SERVER_H
