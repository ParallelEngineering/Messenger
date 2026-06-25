#ifndef MESSENGER_CLIENT_H
#define MESSENGER_CLIENT_H



class client {
public:
    static client& getInstance();
    ~client();

    client(const client&) = delete;
    client& operator=(const client&) = delete;
    client(client&&) = delete;
    client& operator=(client&&) = delete;

private:
    client();
};



#endif //MESSENGER_CLIENT_H
