#include <gtest/gtest.h>
#include "resp/server.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <atomic>
#include <list>
#include <cstdlib>
#include <chrono>

using namespace net::resp;

#define PORT      ntohs(1338)
// 127.0.0.1
#define IP        ntohl(INADDR_LOOPBACK)
#define K_MAX_MSG 10

#define NUM_CONNECTIONS 1

std::vector<std::string> buf_resp(1000);

std::atomic<int> read_idx_resp{0};
std::atomic<int> received_count_resp{0};

void main_loop_resp() {
    RESPServer serv(IP, PORT, K_MAX_MSG);
    while (serv.tcp_accept(
        [](int fd, std::variant<RESPError, std::unique_ptr<data::Node>>&& res) {
            if (std::holds_alternative<std::unique_ptr<data::Node>>(res)) {
                auto& node = std::get<std::unique_ptr<data::Node>>(res);
            } else {
                auto& err = std::get<RESPError>(res);
            }
        }, 
        NUM_CONNECTIONS
    )) {}
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
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) net::die("socket()");

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = PORT;
    addr.sin_addr.s_addr = IP;
    while (connect(fd, (const struct sockaddr*)&addr, sizeof(addr))) {}

    // TODO
}

TEST_F(RESPTest, ParseString) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) net::die("socket()");

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = PORT;
    addr.sin_addr.s_addr = IP;
    while (connect(fd, (const struct sockaddr*)&addr, sizeof(addr))) {}

    // TODO
}

TEST_F(RESPTest, ParseVector) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) net::die("socket()");

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = PORT;
    addr.sin_addr.s_addr = IP;
    while (connect(fd, (const struct sockaddr*)&addr, sizeof(addr))) {}

    // TODO
}

TEST_F(RESPTest, GlobalParse) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) net::die("socket()");

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = PORT;
    addr.sin_addr.s_addr = IP;
    while (connect(fd, (const struct sockaddr*)&addr, sizeof(addr))) {}

    // TODO
}