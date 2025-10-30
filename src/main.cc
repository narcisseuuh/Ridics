#include <string>
#include <iostream>
#include "resp/server.h"

#define PORT      ntohs(1337)
// 127.0.0.1
#define IP        ntohl(INADDR_LOOPBACK)
#define K_MAX_MSG 4096

int main() {
    net::tcp::TCPServerBasic serv(IP, PORT, K_MAX_MSG);
    while (serv.tcp_accept([] (int fd, std::variant<net::tcp::TCPError, std::string> res) {
        std::string s = std::get<std::string>(res);
        std::cout << "recieved : " << s << '\n';
    }, 10)) {}
    return 0;
}