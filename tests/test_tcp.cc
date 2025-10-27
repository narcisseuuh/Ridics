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

#define PORT ntohs(1337)
// 127.0.0.1
#define IP   ntohl(INADDR_LOOPBACK)

#define NUM_CONNECTIONS 1

std::vector<int> buf(1000);

std::atomic<int> read_idx{0};
std::atomic<int> received_count{0};

void main_loop() {
    TCPServer serv(IP, PORT);
    while (serv.tcp_accept([] (int fd, std::string& s) {
        int n;
        while (read(fd, &n, sizeof(int)) > 0) {
            int pos = read_idx.fetch_add(1, std::memory_order_relaxed);
            if (pos < (int)buf.size()) {
                buf[pos] = n;
                received_count.fetch_add(1, std::memory_order_release);
            }
        }
        return 0;
    }, NUM_CONNECTIONS)) {}
    return;
}

class TCPTest : public testing::Test {
protected:
    void SetUp() override {
        read_idx.store(0);
        received_count.store(0);
        _t = std::thread(main_loop);
        _t.detach();
    }

private:
    std::thread _t;
};

TEST_F(TCPTest, FunctioningCommunication) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) net::die("socket()");

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = PORT;
    addr.sin_addr.s_addr = IP;
    int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    if (rv) net::die("connect()");

    srand(time(NULL));
    std::vector<int> msgs(1000);
    int i;
    for (i = 0 ; i < 1000 ; ++i) {
        msgs[i] = rand();
        write(fd, &msgs[i], sizeof(int));
    }

    const auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::milliseconds(2000);
    while (received_count.load(std::memory_order_acquire) < 1000) {
        if (std::chrono::steady_clock::now() - start > timeout) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    int num_eq = 0;
    for (i = 0 ; i < 1000 ; ++i) {
        if (msgs[i] == buf[i]) {
            ++num_eq;
        }
    }
    EXPECT_EQ(num_eq, 1000);
}