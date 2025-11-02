#include "resp/server.h"

void net::die(const std::string& msg) {
    std::cerr << "\033[1;31mfailure : " 
        << msg << "\033[0m" << '\n';
    exit(1);
}

std::variant<net::resp::RESPError, std::unique_ptr<data::Node>>
net::resp::RESPServer::read_string(int connfd, char* body, int& i) {
    // eg.: a simple string corresponding to response code "OK" :
    // +OK\r\n         (for simple string)
    // -ERR blabla\r\n (for simple error)
    ssize_t rv;
    char* str = body + i;
    int i_base = i;
    while (i < net::resp::RESPServer::k_max_msg()) {
        rv = read(connfd, str, 1);
        if (rv <= 0) return {net::resp::ErrKind::END_OF_STREAM};
        // we potentially matched the end of the string \r\n
        if ((*str == '\r') && (i + 1 < net::resp::RESPServer::k_max_msg())) {
            str += 1;
            ++i;
            rv = read(connfd, str, 1);
            if (rv <= 0) return {net::resp::ErrKind::END_OF_STREAM};
            if (*str == '\n') {
                std::string s(body + i_base, i - i_base + 1 - 2);
                ++i;
                return {std::make_unique<data::String>(s)};
            }
        } else {
            return {net::resp::ErrKind::INVALID_CHARACTER};
        }
        ++i;
        str += 1;
    }
    return {net::resp::ErrKind::END_OF_STREAM};
}

std::variant<net::resp::RESPError, std::unique_ptr<data::Node>>
net::resp::RESPServer::read_int(int connfd, char* body, int& i) {
    // eg.: a request corresponding to number 124
    // :124\r\n
    // :+124\r\n (would also work)
    ssize_t rv;
    char* str = body + i;
    std::optional<bool> negative{};
    int i_base = i;
    while (i < net::resp::RESPServer::k_max_msg()) {
        rv = read(connfd, str, 1);
        if (rv <= 0) return {net::resp::ErrKind::INVALID_CHARACTER};
        // we potentially matched the end of the string \r\n
        if ((*str == '\r') && (i + 1 < net::resp::RESPServer::k_max_msg())) {
            str += 1;
            ++i;
            rv = read(connfd, str, 1);
            if (rv <= 0) return {net::resp::ErrKind::INVALID_CHARACTER};
            if (*str == '\n' && i > 2) {
                std::string s(body + i_base, i - i_base + 1 - 2);
                ++i;
                if (negative.has_value() && negative.emplace()) {
                    return {std::unique_ptr<data::Node>(
                        dynamic_cast<data::Node*>(new data::Integer(-static_cast<int64_t>(stoi(s))))
                    )};
                } else {
                    return {std::unique_ptr<data::Node>(
                        dynamic_cast<data::Node*>(new data::Integer(static_cast<int64_t>(stoi(s))))
                    )};
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

std::variant<net::resp::RESPError, std::unique_ptr<data::Node>>
net::resp::RESPServer::read_bulk_string(int connfd, char* body, int& i) {
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
    if (auto* len_variant_ptr = std::get_if<std::unique_ptr<data::Node>>(&len_variant)) {
        if (data::Integer* len_node = dynamic_cast<data::Integer*>(len_variant_ptr->get())) {
            len = len_node->get();
        } else {
            return {net::resp::ErrKind::INVALID_TYPE};
        }
    } else {
        return {net::resp::ErrKind::INVALID_TYPE};
    }

    str = body + i;
    int i_base = i;
    rv = read(connfd, str, len);
    if (rv != len) return net::resp::ErrKind::END_OF_STREAM;
    i += len;
    if (*str == '\r' && i + 1 < net::resp::RESPServer::k_max_msg()) {
        str += 1;
        ++i;
        rv = read(connfd, str, 1);
        if (rv <= 0) return net::resp::ErrKind::END_OF_STREAM;
        if (*str == '\n') {
            std::string s(body + i_base, i - i_base + 1 - 2);
            ++i;
            return {std::unique_ptr<data::Node>(
                dynamic_cast<data::Node*>(new data::BulkString(s))
            )};
        } else {
            return {net::resp::ErrKind::INVALID_CHARACTER};
        }
    } else {
        return {net::resp::ErrKind::INVALID_CHARACTER};
    }
}

std::variant<net::resp::RESPError, std::unique_ptr<data::Node>>
net::resp::RESPServer::read_array(int connfd, char* body, int& i) {
    int64_t len;
    auto len_variant = read_int(connfd, body, i);
    if (std::holds_alternative<net::resp::RESPError>(len_variant)) {
        return std::get<net::resp::RESPError>(len_variant);
    }
    if (auto* len_variant_ptr = std::get_if<std::unique_ptr<data::Node>>(&len_variant)) {
        if (data::Integer* len_node = dynamic_cast<data::Integer*>(len_variant_ptr->get())) {
            len = len_node->get();
        } else {
            return {net::resp::ErrKind::INVALID_TYPE};
        }
    } else {
        return {net::resp::ErrKind::INVALID_TYPE};
    }

    data::Array* a = new data::Array(len);
    for (int k = 0 ; k < len ; ++k) {
        auto req = one_request(connfd, body, i);
        if (auto node_ptr = std::get_if<std::unique_ptr<data::Node>>(&req)) {
            a->push_back(std::move(*node_ptr));
        } else {
            return std::get<net::resp::RESPError>(req);
        }
    }
    auto res = std::unique_ptr<data::Node>(
        dynamic_cast<data::Node*>(a)
    );
    return {std::move(res)};
}

std::optional<net::resp::RESPError> 
net::resp::RESPServer::handshake(int connfd) {
    // TOCHANGE : more proper parsing including authentification
    std::vector<char> body(k_max_msg());
    int32_t rv;

    rv = read_stream(connfd, body.data(), 6);
    if (rv <= 0) return {net::resp::ErrKind::END_OF_STREAM};
    if (std::string(body.data(), 6) == "HELLO 3") {
        return {};
    }
    return {net::resp::ErrKind::END_OF_STREAM};
}

std::variant<net::resp::RESPError, std::unique_ptr<data::Node>>
net::resp::RESPServer::one_request(
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
            return net::resp::RESPServer::read_string(connfd, body, i);
        // simple error
        case '-':
            return net::resp::RESPServer::read_string(connfd, body, i);
        // integer
        case ':':
            return net::resp::RESPServer::read_int(connfd, body, i);
        // bulk string
        case '$':
            return net::resp::RESPServer::read_bulk_string(connfd, body, i);
        // array
        case '*':
            return net::resp::RESPServer::read_array(connfd, body, i);
        // unknown
        default:
            return {net::resp::ErrKind::UNHANDLED};
    } 
}