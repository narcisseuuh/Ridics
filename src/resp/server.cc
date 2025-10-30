#include "resp/server.h"
#include <iostream>
#include <functional>
#include <variant>
#include <optional>

void net::die(const std::string& msg) {
    std::cerr << "\033[1;31mfailure : " 
        << msg << "\033[0m" << '\n';
    exit(1);
}

template <typename... Types>
std::variant<net::resp::RESPError, std::string>
net::resp::RESPServer<Types...>::read_string(int connfd, char* body, int& i) {
    // eg.: a simple string corresponding to response code "OK" :
    // +OK\r\n         (for simple string)
    // -ERR blabla\r\n (for simple error)
    ssize_t rv;
    char* str = body + i;
    int i_base = i;
    while (i < net::resp::RESPServer<Types...>::k_max_msg()) {
        rv = read(connfd, str, 1);
        if (rv <= 0) return {net::resp::ErrKind::END_OF_STREAM};
        // we potentially matched the end of the string \r\n
        if ((*str == '\r') && (i + 1 < net::resp::RESPServer<Types...>::k_max_msg())) {
            str += 1;
            ++i;
            rv = read(connfd, str, 1);
            if (rv <= 0) return {net::resp::ErrKind::END_OF_STREAM};
            if (*str == '\n') {
                std::string res(body + i_base, i - i_base + 1 - 2);
                ++i;
                return res;
            }
        } else {
            return {net::resp::ErrKind::INVALID_CHARACTER};
        }
        ++i;
        str += 1;
    }
    return {net::resp::ErrKind::END_OF_STREAM};
}

template <typename... Types>
std::variant<net::resp::RESPError, int64_t>
net::resp::RESPServer<Types...>::read_int(int connfd, char* body, int& i) {
    // eg.: a request corresponding to number 124
    // :124\r\n
    // :+124\r\n (would also work)
    ssize_t rv;
    char* str = body + i;
    std::optional<bool> negative{};
    int i_base = i;
    while (i < net::resp::RESPServer<Types...>::k_max_msg()) {
        rv = read(connfd, str, 1);
        if (rv <= 0) return {net::resp::ErrKind::INVALID_CHARACTER};
        // we potentially matched the end of the string \r\n
        if ((*str == '\r') && (i + 1 < net::resp::RESPServer<Types...>::k_max_msg())) {
            str += 1;
            ++i;
            rv = read(connfd, str, 1);
            if (rv <= 0) return {net::resp::ErrKind::INVALID_CHARACTER};
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
            return {net::resp::ErrKind::INVALID_CHARACTER};
        }
        ++i;
        str += 1;
    }
    return {net::resp::ErrKind::INVALID_CHARACTER};
}

template <typename... Types>
std::variant<net::resp::RESPError, std::string>
net::resp::RESPServer<Types...>::read_bulk_string(int connfd, char* body, int& i) {
    // eg.: a request corresponding to "test"
    // $4\r\ntest\r\n
    // NOTE: $0\r\n\r\n == ""
    ssize_t rv;
    char* str;
    
    // reading string length
    auto len_variant = read_int(connfd, body, i);
    int64_t len = 0;
    if (std::holds_alternative<net::resp::RESPError>(len_variant)) {
        return std::get<net::resp::RESPError>(len_variant);
    }
    len = std::get<int64_t>(len_variant);

    str = body + i;
    int i_base = i;
    rv = read(connfd, str, len);
    if (rv != len) return net::resp::ErrKind::END_OF_STREAM;
    i += len;
    if (*str == '\r' && i + 1 < net::resp::RESPServer<Types...>::k_max_msg()) {
        str += 1;
        ++i;
        rv = read(connfd, str, 1);
        if (rv <= 0) return net::resp::ErrKind::END_OF_STREAM;
        if (*str == '\n') {
            std::string res(body + i_base, i - i_base + 1 - 2);
            ++i;
            return res;
        } else {
            return {net::resp::ErrKind::INVALID_CHARACTER};
        }
    } else {
        return {net::resp::ErrKind::INVALID_CHARACTER};
    }
}

template <typename... Types>
std::variant<net::resp::RESPError, std::vector<Types>...>
net::resp::RESPServer<Types...>::read_array(int connfd, char* body, int& i) {
    int64_t len;
    auto len_variant = read_int(connfd, body, i);
    if (std::holds_alternative<net::resp::RESPError>(len_variant)) {
        return std::get<net::resp::RESPError>(len_variant);
    }
    len = std::get<int64_t>(len_variant);

    // we have to take case 0 first to have the type of the vector elements
    auto first_req = net::resp::RESPServer<Types...>::one_request(connfd, body, i);
    if (std::holds_alternative<net::resp::RESPError>(first_req)) {
        return std::get<net::resp::RESPError>(first_req);
    }
    // then we can define our vector and iterate on the rest of elements
    using T = decltype(first_req);
    std::vector<T> res(len);
    res[0] = std::get<T>(first_req);
    for (int k = 1 ; k < len ; ++k) {
        auto req = one_request(connfd, body, i);
        if (std::holds_alternative<T>(req)) {
            res[k] = std::get<T>(req);
        } else {
            return std::get<net::resp::RESPError>(req);
        }
    }
    return res;
}

template <typename... Types>
std::optional<net::resp::RESPError> net::resp::RESPServer<Types...>::handshake(int connfd) {
    // TOCHANGE : more proper parsing including authentification
    char body[net::resp::RESPServer<Types...>::k_max_msg()];
    int32_t rv;

    rv = read_stream(connfd, body, 6);
    if (rv <= 0) return {net::resp::ErrKind::END_OF_STREAM};
    if (std::string(body, 6) == "HELLO 3") {
        return {};
    }
    return {net::resp::ErrKind::END_OF_STREAM};
}

template <typename... Types>
std::variant<net::resp::RESPError, Types...>
net::resp::RESPServer<Types...>::one_request(
    int connfd,
    char* body,
    int& i
) {
    /* implementation of the RESP protocol, based on the official documentation.
    * see more at https://redis.io/docs/latest/develop/reference/protocol-spec/
    */
    int32_t err = read_stream(connfd, body + i, 1);
    i += 1;
    if (err) return {net::resp::ErrKind::END_OF_STREAM};
    switch (body[i]) {
        // simple string
        case '+':
            return net::resp::RESPServer<Types...>::read_string(connfd, body, i);
        // simple error
        case '-':
            return net::resp::RESPServer<Types...>::read_string(connfd, body, i);
        // integer
        case ':':
            return net::resp::RESPServer<Types...>::read_int(connfd, body, i);
        // bulk string
        case '$':
            return net::resp::RESPServer<Types...>::read_bulk_string(connfd, body, i);
        // array
        case '*':
            return net::resp::RESPServer<Types...>::read_array(connfd, body, i);
        // unknown
        default:
            return {net::resp::ErrKind::UNHANDLED};
    } 
}