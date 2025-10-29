#include "resp/server.h"
#include <iostream>
#include <functional>
#include <variant>
#include <optional>

void net::die(const std::string& msg) {
    std::cerr << "failure : " << msg << '\n';
    exit(1);
}

static inline int32_t net::read_stream(int fd, char* buf, size_t n) {
    ssize_t rv;
    while (n > 0) {
        rv = read(fd, buf, n);
        if (rv <= 0) return -1;
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static inline int32_t net::write_stream(int fd, char* buf, size_t n) {
    ssize_t rv;
    while (n > 0) {
        rv = write(fd, buf, n);
        if (rv <= 0) return -1;
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static inline std::variant<int, std::string>
read_string(int connfd, char* body, int _k_max_msg, int& i) {
    // eg.: a simple string corresponding to response code "OK" :
    // +OK\r\n         (for simple string)
    // -ERR blabla\r\n (for simple error)
    ssize_t rv;
    char* str = body + i;
    int i_base = i;
    while (i < _k_max_msg) {
        rv = read(connfd, str, 1);
        if (rv <= 0) return -1;
        // we potentially matched the end of the string \r\n
        if ((*str == '\r') && (i + 1 < _k_max_msg)) {
            str += 1;
            ++i;
            rv = read(connfd, str, 1);
            if (rv <= 0) return -1;
            if (*str == '\n') {
                std::string res(body + i_base, i - i_base + 1 - 2);
                ++i;
                return res;
            }
        } else {
            return -1;
        }
        ++i;
        str += 1;
    }
    return -1;
}

static inline std::variant<int, int64_t>
read_int(int connfd, char* body, int _k_max_msg, int& i) {
    // eg.: a request corresponding to number 124
    // :124\r\n
    // :+124\r\n (would also work)
    ssize_t rv;
    char* str = body + i;
    std::optional<bool> negative{};
    int i_base = i;
    while (i < _k_max_msg) {
        rv = read(connfd, str, 1);
        if (rv <= 0) return -1;
        // we potentially matched the end of the string \r\n
        if ((*str == '\r') && (i + 1 < _k_max_msg)) {
            str += 1;
            ++i;
            rv = read(connfd, str, 1);
            if (rv <= 0) return -1;
            if (*str == '\n' && i > 2) {
                std::string res(body + i_base, i - i_base + 1 - 2);
                ++i;
                if (negative.has_value() && negative.emplace()) {
                    return -static_cast<int64_t>(stoi(res));
                } else {
                    return static_cast<int64_t>(stoi(res));
                }
            }
        } else if ((*str == '-' || *str == '+') && (i == 1)) {
            // beginning with +/-
            negative = (*str == '-');
        } else {
            return -1;
        }
        ++i;
        str += 1;
    }
    return -1;
}

static inline std::variant<int, std::string>
read_bulk_string(int connfd, char* body, int _k_max_msg, int& i) {
    // eg.: a request corresponding to "test"
    // $4\r\ntest\r\n
    // NOTE: $0\r\n\r\n == ""
    ssize_t rv;
    char* str;
    
    // reading string length
    auto len_variant = read_int(connfd, body, _k_max_msg, i);
    int64_t len = 0;
    if (std::holds_alternative<int32_t>(len_variant)) {
        return -1;
    }
    len = std::get<int64_t>(len_variant);

    str = body + i;
    int i_base = i;
    rv = read(connfd, str, len);
    if (rv != len) return -1;
    i += len;
    if (*str == '\r' && i + 1 < _k_max_msg) {
        str += 1;
        ++i;
        rv = read(connfd, str, 1);
        if (rv <= 0) return -1;
        if (*str == '\n') {
            std::string res(body + i_base, i - i_base + 1 - 2);
            ++i;
            return res;
        } else {
            return -1;
        }
    } else {
        return -1;
    }
}

template <typename... Types>
static inline std::variant<int32_t, std::vector<Types>...>
read_array(int connfd, char* body, int _k_max_msg, int& i) {
    int64_t len;
    auto len_variant = read_int(connfd, body, _k_max_msg, i);
    if (std::holds_alternative<int32_t>(len_variant)) {
        return -1;
    }
    len = std::get<int64_t>(len_variant);

    // TODO
    return -1;
}

template <typename... Types>
int net::resp::RESPServer<Types...>::handshake(int connfd) {
    // TOCHANGE : more proper parsing including authentification
    char body[_k_max_msg];
    int32_t rv;

    rv = read_stream(connfd, body, 6);
    if (rv <= 0) return -1;
    if (std::string(body, 6) == "HELLO 3") {
        return 0;
    }
    return -1;
}

template <typename... Types>
std::variant<int32_t, Types...>
net::resp::RESPServer<Types...>::one_request(
    int connfd,
    int& i
) {
    /* implementation of the RESP protocol, based on the official documentation.
    * see more at https://redis.io/docs/latest/develop/reference/protocol-spec/
    */
    char body[_k_max_msg];
    int32_t err = read_stream(connfd, body + i, 1);
    if (err) return -1;
    switch (body[i]) {
        // simple string
        case '+':
            break;
        // simple error
        case '-':
            break;
        // integer
        case ':':
            break;
        // bulk string
        case '$':
            break;
        // array
        case '*':
            break;
        // null
        case '_':
            break;
        // boolean
        case '#':
            break;
        // double
        case ',':
            break;
        // big number
        case '(':
            break;
        // bulk error
        case '!':
            break;
        // verbatim string
        case '=':
            break;
        // map
        case '%':
            break;
        // attribute
        case '|':
            break;
        // set
        case '~':
            break;
        // push
        case '>':
            break;
        // unknown
        default:
            break;
    } 
}