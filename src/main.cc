#include <string>
#include <iostream>
#include "resp/handle.h"
#include "resp/resp_utils.h"

#define PORT      ntohs(1337)
// 127.0.0.1
#define IP        ntohl(INADDR_LOOPBACK)
#define K_MAX_MSG 4096

int main() {
    net::resp::RESPServer serv(IP, PORT, K_MAX_MSG);
    using Output = std::unique_ptr<data::Node>;
    while (serv.tcp_accept([] (int fd, std::variant<net::resp::RESPError, Output>&& res) {
        if (auto* err = std::get_if<net::resp::RESPError>(&res)) {
            std::string err_msg = err->to_string();
            std::string response = net::resp::err(err_msg);
            std::cout << response << '\n';
            net::write_stream(fd, response.c_str(), response.size());
        } else {
            std::string ok_msg = net::resp::ok();
            net::write_stream(fd, ok_msg.c_str(), ok_msg.size());
        }
    }, 10)) {}
    return 0;
}