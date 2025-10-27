#include <string>
#include <iostream>
#include "http/server.h"

#define PORT ntohs(1337)
// 127.0.0.1
#define IP   ntohl(INADDR_LOOPBACK)

int main() {
    net::tcp::TCPServer serv(IP, PORT);
    while (serv.tcp_accept([] (int fd) {
        char nbuf[64] = {};
        ssize_t n = read(fd, &nbuf, sizeof(nbuf) - 1);
        if (n < 0) net::die("read()");
        std::cout << "recieved : " << nbuf << '\n';
    }, 10)) {}
    return 0;
}