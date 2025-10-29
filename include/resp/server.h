#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <functional>
#include <variant>

namespace net {

void die(const std::string& msg);
static inline int32_t read_stream(int fd, char* buf, size_t n);
static inline int32_t write_stream(int fd, char* buf, size_t n);

namespace tcp {

template<typename Derived, typename... Types>
class TCPServer {
public:
    explicit TCPServer(unsigned int s_addr, unsigned short port) {
        _fd = socket(AF_INET, SOCK_STREAM, 0);
        if (_fd < 0) die("socket()");
        _serverAddress = {};
        _serverAddress.sin_family = AF_INET;
        _serverAddress.sin_port = port;
        _serverAddress.sin_addr.s_addr = s_addr;
        int rv = bind(_fd, (struct sockaddr*)&_serverAddress, sizeof(_serverAddress));
        if (rv) die("bind()");
    }

    using worker_t = std::function<int(int, std::variant<Types&...>)>;
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
            while (true) {
                auto res = static_cast<Derived*>(this)->template one_request<int32_t, Types...>(connfd, 0);
                if (std::holds_alternative<int32_t>(res)) break;
                w(res);
            }
            close(connfd);
            --n;
        }
        return 0;
    }

    int tcp_accept_all(worker_t w) {
        int connfd;
        while (true) {
            struct sockaddr_in client_addr = {};
            socklen_t addrlen = sizeof(client_addr);
            connfd = accept(_fd, (struct sockaddr*)&client_addr, &addrlen);
            if (connfd < 0) {
                continue;
            }
            while (true) {
                auto res = static_cast<Derived*>(this)->template one_request<int32_t, Types...>(connfd, 0);
                if (std::holds_alternative<int32_t>(res)) break;
                w(res);
            }
            close(connfd);
        }
        return 0;
    }

    ~TCPServer() {
        if (_fd >= 0) close(_fd);
    }
private:
    int _fd;
    sockaddr_in _serverAddress;
};

} // namespace tcp

namespace resp {

template <typename... Types>
class RESPServer : public net::tcp::TCPServer<RESPServer<Types...>, Types...> {
public:
    explicit RESPServer(unsigned int s_addr, unsigned short port, const int k_max_msg) 
        : _k_max_msg(k_max_msg), 
          net::tcp::TCPServer<RESPServer<Types...>, Types...>(s_addr, port) {}

    using worker_t = std::function<int(int, std::variant<Types&...>)>;
    std::variant<int32_t, Types...> one_request(int connfd, int& i);

    int handshake(int connfd);

private:
    const int _k_max_msg;
};

} // namespace resp

} // namespace net