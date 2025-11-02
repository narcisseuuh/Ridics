#include <gtest/gtest.h>
#include "resp/server.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <atomic>
#include <list>
#include <cstdlib>
#include <chrono>

using namespace net::tcp;

#define PORT      ntohs(1337)
// 127.0.0.1
#define IP        ntohl(INADDR_LOOPBACK)
#define K_MAX_MSG 10

#define NUM_CONNECTIONS 1

std::vector<std::string> buf_tcp(1000);

std::atomic<int> read_idx_tcp{0};
std::atomic<int> received_count_tcp{0};

void main_loop_tcp() {
    TCPServerBasic serv(IP, PORT, K_MAX_MSG);
    while (serv.tcp_accept([] (int fd, std::variant<TCPError, std::string> res) {
        std::string s = std::get<std::string>(res);
        if (s.size() == 0) return;
        int pos = read_idx_tcp.fetch_add(1, std::memory_order_relaxed);
        if (pos < (int)buf_tcp.size()) {
            buf_tcp[pos] = s;
            received_count_tcp.fetch_add(1, std::memory_order_release);
        }
        return;
    }, NUM_CONNECTIONS)) {}
    return;
}

class TCPTest : public testing::Test {
protected:
    void SetUp() override {
        read_idx_tcp.store(0);
        received_count_tcp.store(0);
        _t = std::thread(main_loop_tcp);
    }

    void TearDown() override {
        pthread_cancel(_t.native_handle());
        _t.join();
    }

private:
    std::thread _t;
};

TEST_F(TCPTest, FunctioningCommunication) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) net::die("socket()");

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = PORT;
    addr.sin_addr.s_addr = IP;
    while (connect(fd, (const struct sockaddr*)&addr, sizeof(addr))) {}

    srand(time(NULL));
    std::vector<std::string> msgs(1000);
    char newline = '\n';
    int i;
    for (i = 0 ; i < 1000 ; ++i) {
        msgs[i] = std::to_string(rand());
        auto it = msgs[i].begin();
        net::write_stream(fd, &*it, msgs[i].size());
        net::write_stream(fd, &newline, 1);
    }

    const auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::milliseconds(2000);
    while (received_count_tcp.load(std::memory_order_acquire) < 1000) {
        if (std::chrono::steady_clock::now() - start > timeout) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    int num_eq = 0;
    for (i = 0 ; i < 1000 ; ++i) {
        if (msgs[i] == buf_tcp[i]) {
            ++num_eq;
        }
    }
    EXPECT_EQ(num_eq, 1000);
}