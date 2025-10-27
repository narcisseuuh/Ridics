#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <functional>

namespace net {

void die(const std::string& msg);

namespace tcp {

class TCPServer {
public:
    TCPServer(unsigned int s_addr, unsigned short port) {
        _fd = socket(AF_INET, SOCK_STREAM, 0);
        if (_fd < 0) die("socket()");
        _serverAddress = {};
        _serverAddress.sin_family = AF_INET;
        _serverAddress.sin_port = port;
        _serverAddress.sin_addr.s_addr = s_addr;
        int rv = bind(_fd, (struct sockaddr*)&_serverAddress, sizeof(_serverAddress));
        if (rv) die("bind()");
    }

    using worker_t = std::function<void(int)>;
    int tcp_accept(worker_t w, int n) {
        int connfd;
        listen(_fd, n);
        while (n > 0) {
            struct sockaddr_in client_addr = {};
            socklen_t addrlen = sizeof(client_addr);
            connfd = accept(_fd, (struct sockaddr*)&client_addr, &addrlen);
            if (connfd < 0) {
                continue;
            }
            w(connfd);
            close(connfd);
            --n;
        }
        return 0;
    }

    ~TCPServer() {}
private:
    int _fd;
    sockaddr_in _serverAddress;
};

} // namespace tcp

namespace http {

} // namespace http

} // namespace net