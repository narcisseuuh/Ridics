#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <variant>
#include "datastructures/node.h"

namespace net {

void die(const std::string& msg);
static inline int32_t read_stream(int fd, char* buf, size_t n) {
    ssize_t rv;
    while (n > 0) {
        rv = read(fd, buf, n);
        if (rv <= 0) return -1;
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static inline int32_t write_stream(int fd, char* buf, size_t n) {
    ssize_t rv;
    while (n > 0) {
        rv = write(fd, buf, n);
        if (rv <= 0) return -1;
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

namespace tcp {

// TCP Server interface (using CRTP)
template<typename Derived, typename Err, typename... Types>
class TCPServer {
public:
    explicit TCPServer(unsigned int s_addr, unsigned short port, const int k_max_msg)
        : _k_max_msg(k_max_msg) {
        _fd = socket(AF_INET, SOCK_STREAM, 0);
        if (_fd < 0) die("socket()");
        _serverAddress = {};
        _serverAddress.sin_family = AF_INET;
        _serverAddress.sin_port = port;
        _serverAddress.sin_addr.s_addr = s_addr;
        int rv = bind(_fd, (struct sockaddr*)&_serverAddress, sizeof(_serverAddress));
        if (rv) die("bind()");
    }

    using worker_t = std::function<void(int, std::variant<Err, Types...>&&)>;

    int tcp_accept(worker_t w, int n) {
        // accepting n clients
        int connfd;
        listen(_fd, n);
        while (n > 0) {
            struct sockaddr_in client_addr = {};
            socklen_t addrlen = sizeof(client_addr);
            connfd = accept(_fd, (struct sockaddr*)&client_addr, &addrlen);
            if (connfd < 0) {
                continue;
            }
            auto handshake = static_cast<Derived*>(this)->handshake(connfd);
            if (handshake.has_value()) {
                handshake.value()();
                break;
            }
            while (true) {
                char body[_k_max_msg];
                int i = 0;
                auto res = static_cast<Derived*>(this)->template one_request(connfd, body, i);
                if (std::holds_alternative<Err>(res)) {
                    std::get<Err>(res)();
                    break;
                }
                w(connfd, std::move(res));
            }
            close(connfd);
            --n;
        }
        return 0;
    }

    int tcp_accept_all(worker_t w) {
        // accepting all clients
        int connfd;
        while (true) {
            struct sockaddr_in client_addr = {};
            socklen_t addrlen = sizeof(client_addr);
            connfd = accept(_fd, (struct sockaddr*)&client_addr, &addrlen);
            if (connfd < 0) {
                continue;
            }
            auto handshake = static_cast<Derived*>(this)->handshake(connfd);
            if (handshake.has_value()) {
                handshake.value()();
                break;
            }
            while (true) {
                char body[_k_max_msg];
                int i = 0;
                auto res = static_cast<Derived*>(this)->template one_request(connfd, body, i);
                if (std::holds_alternative<Err>(res)) {
                    std::get<Err>(res)();
                    break;
                }
                w(connfd, std::move(res));
            }
            close(connfd);
        }
        return 0;
    }

    const int k_max_msg() {
        return _k_max_msg;
    }

    ~TCPServer() {
        if (_fd >= 0) close(_fd);
    }
private:
    int _fd;
    sockaddr_in _serverAddress;
    const int _k_max_msg;
};

enum ErrKind {
    READ_FAILURE
};

struct TCPError {
    ErrKind _err;

    TCPError(ErrKind err) : _err(err) {}

    void operator()() const {
        std::string err_msg;
        switch (_err) {
            case READ_FAILURE:
                err_msg = "failure due to read()";
                break;
        }
        std::cerr << "\033[1;31m"
            << "Failed to parse request : "
            << err_msg << "\033[0m" << '\n';
    }
};

// basic TCP Server implementation for testing purpose
class TCPServerBasic : public TCPServer<TCPServerBasic, TCPError, std::string> {
public:
    TCPServerBasic(unsigned int s_addr, unsigned short port, const int k_max_msg) 
        : net::tcp::TCPServer<TCPServerBasic, TCPError, std::string>(s_addr, port, k_max_msg) {}

    std::variant<TCPError, std::string> one_request(int connfd, char* body, int& i) {
        // Read up to k_max_msg() bytes, looking for newline as message delimiter
        ssize_t n = 0;
        const int max_size = this->k_max_msg();
        
        while (n < max_size) {
            char byte;
            int rv = read_stream(connfd, &byte, 1);
            if (rv < 0) {
                return {net::tcp::ErrKind::READ_FAILURE};
            }
            body[n] = byte;
            ++n;
            if (byte == '\n') {
                break;
            }
        }
        return {std::string(body, n > 0 && body[n-1] == '\n' ? n - 1 : n)};
    }

    std::optional<TCPError> handshake(int connfd) {
        // no handshake needed by default
        return {};
    }
};

} // namespace tcp

namespace resp {

enum ErrKind {
    INVALID_CHARACTER,
    END_OF_STREAM,
    INVALID_TYPE,
    UNHANDLED
};

struct RESPError {
    ErrKind _err;

    RESPError(ErrKind err) : _err(err) {}

    void operator()() const {
        std::string err_msg;
        switch (_err) {
            case INVALID_CHARACTER:
                err_msg = "invalid character detected";
                break;
            case END_OF_STREAM:
                err_msg = "unexpected end of message";
                break;
            case INVALID_TYPE:
                err_msg = "mismatching type for container";
                break;
            case UNHANDLED:
                err_msg = "currently unhandled";
                break;
        }
        std::cerr << "\033[1;31m"
            << "Failed to parse request : "
            << err_msg << "\033[0m" << '\n';
    }
};

class RESPServer : public net::tcp::TCPServer<RESPServer, RESPError, std::unique_ptr<data::Node>> {
public:
    explicit RESPServer(unsigned int s_addr, unsigned short port, const int k_max_msg) 
        : net::tcp::TCPServer<RESPServer, RESPError, std::unique_ptr<data::Node>>(s_addr, port, k_max_msg) {}

    std::variant<RESPError, std::unique_ptr<data::Node>> read_array(int connfd, char* body, int& i);
    std::variant<RESPError, std::unique_ptr<data::Node>> read_bulk_string(int connfd, char* body, int& i);
    std::variant<RESPError, std::unique_ptr<data::Node>> read_int(int connfd, char* body, int& i);
    std::variant<RESPError, std::unique_ptr<data::Node>> read_string(int connfd, char* body, int& i);

    std::variant<RESPError, std::unique_ptr<data::Node>> one_request(int connfd, char*body, int& i);

    std::optional<net::resp::RESPError> handshake(int connfd);
};

} // namespace resp

} // namespace net