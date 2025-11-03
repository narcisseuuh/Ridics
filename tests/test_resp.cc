#include <gtest/gtest.h>
#include "resp/server.h"
#include "resp/resp_utils.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <atomic>
#include <list>
#include <cstdlib>
#include <chrono>

using namespace net::resp;

#define PORT      ntohs(1338)
#define IP        ntohl(INADDR_LOOPBACK)
#define K_MAX_MSG 512

#define NUM_CONNECTIONS 1

inline void write_view(int fd, std::string_view sv, int n) {
    net::write_stream(fd, std::string(sv).c_str(), n);
}

std::vector<data::Node*> buf_resp(1000);

std::atomic<int> read_idx_resp{0};
std::atomic<int> received_count_resp{0};

void main_loop_resp() {
    RESPServer serv(IP, PORT, K_MAX_MSG);
    
    while (serv.tcp_accept([] (int fd, std::variant<RESPError, std::unique_ptr<data::Node>>&& res) {
        if (std::holds_alternative<std::unique_ptr<data::Node>>(res)) {
                auto node = std::move(std::get<std::unique_ptr<data::Node>>(res));
                std::string ok_msg = net::resp::ok();
                write_view(fd, ok_msg, ok_msg.size());
            } else {
                auto err = std::get<RESPError>(res);
                std::string err_msg = err.to_string();
                write_view(fd, net::resp::err(err_msg), net::resp::err(err_msg).size());
            }
    }, NUM_CONNECTIONS)) {}
    return;
}

class RESPTest : public testing::Test {
protected:
    void SetUp() override {
        read_idx_resp.store(0);
        received_count_resp.store(0);
        _t = std::thread(main_loop_resp);
    }

    void TearDown() override {
        pthread_cancel(_t.native_handle());
        _t.join();
    }

private:
    std::thread _t;
};

TEST_F(RESPTest, ParseInt) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(fd, 0) << "socket() failed";

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = PORT;
    addr.sin_addr.s_addr = IP;
    
    int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    ASSERT_EQ(rv, 0) << "connect() failed";

    // handshake
    write_view(fd, "HELLO 3\r\n", sizeof("HELLO 3\r\n"));

    write_view(fd, ":123\r\n", sizeof(":123\r\n"));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    close(fd);
}

TEST_F(RESPTest, ParseString) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(fd, 0) << "socket() failed";

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = PORT;
    addr.sin_addr.s_addr = IP;
    
    int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    ASSERT_EQ(rv, 0) << "connect() failed";

    // handshake
    write_view(fd, "HELLO 3\r\n", sizeof("HELLO 3\r\n"));

    write_view(fd, "+OK\r\n", sizeof("+OK\r\n"));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    close(fd);
}

TEST_F(RESPTest, ParseBulkString) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(fd, 0) << "socket() failed";

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = PORT;
    addr.sin_addr.s_addr = IP;
    
    int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    ASSERT_EQ(rv, 0) << "connect() failed";

    // handshake
    write_view(fd, "HELLO 3\r\n", sizeof("HELLO 3\r\n"));

    write_view(fd, "$4\r\ntest\r\n", sizeof("$4\r\ntest\r\n"));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    close(fd);
}

TEST_F(RESPTest, ParseArray) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(fd, 0) << "socket() failed";

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = PORT;
    addr.sin_addr.s_addr = IP;
    
    int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    ASSERT_EQ(rv, 0) << "connect() failed";

    // handshake
    write_view(fd, "HELLO 3\r\n", sizeof("HELLO 3\r\n"));

    write_view(fd, "*2\r\n:123\r\n+OK\r\n", sizeof("*2\r\n:123\r\n+OK\r\n"));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    close(fd);
}