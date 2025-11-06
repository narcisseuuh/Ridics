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
#include <vector>
#include <memory>
#include <string>
#include <cstring>

using namespace net::resp;

#define PORT      ntohs(1338)
#define IP        ntohl(INADDR_LOOPBACK)
#define K_MAX_MSG 512

#define NUM_CONNECTIONS 1

inline void write_view(int fd, std::string_view sv, int n) {
    net::write_stream(fd, std::string(sv).c_str(), n);
}

std::vector<std::unique_ptr<data::Node>> buf_resp(1000);

std::atomic<int> read_idx_resp{0};
std::atomic<int> received_count_resp{0};

void main_loop_resp() {
    RESPServer serv(IP, PORT, K_MAX_MSG);

    while (serv.tcp_accept([] (int fd, std::variant<RESPError, std::unique_ptr<data::Node>>&& res) {
        if (std::holds_alternative<std::unique_ptr<data::Node>>(res)) {
            auto node = std::move(std::get<std::unique_ptr<data::Node>>(res));
            int pos = read_idx_resp.fetch_add(1, std::memory_order_relaxed);
            if (pos < (int)buf_resp.size()) {
                buf_resp[pos] = std::move(node);
            }
            received_count_resp.fetch_add(1, std::memory_order_release);
            std::string ok_msg = net::resp::ok();
            net::write_stream(fd, ok_msg.c_str(), ok_msg.size());
        } else {
            auto err = std::get<RESPError>(res);
            std::string err_msg = err.to_string();
            std::cerr << "Server parse error: " << err_msg << '\n';
            std::string em = net::resp::err(err_msg);
            net::write_stream(fd, em.c_str(), em.size());
        }
        return 0;
    }, NUM_CONNECTIONS)) {}
    return;
}

class RESPTest : public testing::Test {
protected:
    void SetUp() override {
        read_idx_resp.store(0, std::memory_order_relaxed);
        received_count_resp.store(0, std::memory_order_relaxed);
        for (auto &p : buf_resp) p.reset();
        _t = std::thread(main_loop_resp);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override {
        pthread_cancel(_t.native_handle());
        _t.join();
    }

private:
    std::thread _t;
};

static std::string read_n(int fd, size_t n, std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
    std::vector<char> buf(n);
    auto start = std::chrono::steady_clock::now();
    while (true) {
        int32_t rv = net::read_stream(fd, buf.data(), n);
        if (std::chrono::steady_clock::now() - start > timeout) break;
        if (rv < 0) continue;
        return std::string(buf.data(), n);
    }
    return std::string();
}

static inline std::string generate_random(size_t max_sz) {
    // generation of a random buffer
    size_t sz = rand() % max_sz;
    std::string res = std::format("*{}\r\n", sz);
    std::string bs;
    uint16_t bs_len;
    int16_t k;
    for (int i = 0 ; i < sz ; ++i) {
        int choice = rand() % 3;
        switch (choice) {
            case 0:
                // integer
                k = rand();
                res += std::format(":{}\r\n", k);
                break;
            case 1:
                // bulk string
                bs_len = rand();
                for (int j = 0 ; j < bs_len ; ++j) {
                    bs += std::to_string(rand());
                }
                res += bs;
                break;
            case 2:
                // array
                res += generate_random(max_sz / 2);
                break;
        }
    }
    if (res.size() < K_MAX_MSG) return res;
    else return generate_random(max_sz / 2);
}

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
    write_view(fd, "HELLO 3\r\n", sizeof("HELLO 3\r\n") - 1);
    std::string hresp = read_n(fd, 5);
    ASSERT_EQ(hresp, "+OK\r\n") << "Handshake failed: " << hresp;

    const std::string sent = ":123\r\n";
    write_view(fd, sent, sent.size());

    std::string ack = read_n(fd, 5);
    ASSERT_EQ(ack, "+OK\r\n") << "Server did not ack integer: " << ack;

    const auto start = std::chrono::steady_clock::now();
    while (received_count_resp.load(std::memory_order_acquire) < 1) {
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(2)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_GE(received_count_resp.load(), 1) << "Server did not receive integer";

    auto &node = buf_resp[0];
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->to_resp(), sent);

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
    write_view(fd, "HELLO 3\r\n", sizeof("HELLO 3\r\n") - 1);
    std::string hresp = read_n(fd, 5);
    ASSERT_EQ(hresp, "+OK\r\n") << "Handshake failed: " << hresp;

    const std::string sent = "+OK\r\n";
    write_view(fd, sent, sent.size());

    std::string ack = read_n(fd, 5);
    ASSERT_EQ(ack, "+OK\r\n") << "Server did not ack simple string: " << ack;

    const auto start = std::chrono::steady_clock::now();
    while (received_count_resp.load(std::memory_order_acquire) < 1) {
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(2)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_GE(received_count_resp.load(), 1) << "Server did not receive simple string";
    auto &node = buf_resp[0];
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->to_resp(), sent);

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
    write_view(fd, "HELLO 3\r\n", sizeof("HELLO 3\r\n") - 1);
    std::string hresp = read_n(fd, 5);
    ASSERT_EQ(hresp, "+OK\r\n") << "Handshake failed: " << hresp;

    const std::string sent = "$4\r\ntest\r\n";
    write_view(fd, sent, sent.size());

    std::string ack = read_n(fd, 5);
    ASSERT_EQ(ack, "+OK\r\n") << "Server did not ack bulk string: " << ack;

    const auto start = std::chrono::steady_clock::now();
    while (received_count_resp.load(std::memory_order_acquire) < 1) {
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(2)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_GE(received_count_resp.load(), 1) << "Server did not receive bulk string";
    auto &node = buf_resp[0];
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->to_resp(), sent);

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
    write_view(fd, "HELLO 3\r\n", sizeof("HELLO 3\r\n") - 1);
    std::string hresp = read_n(fd, 5);
    ASSERT_EQ(hresp, "+OK\r\n") << "Handshake failed: " << hresp;

    const std::string sent = "*2\r\n:123\r\n+OK\r\n";
    write_view(fd, sent, sent.size());

    std::string ack = read_n(fd, 5);
    ASSERT_EQ(ack, "+OK\r\n") << "Server did not ack array: " << ack;

    const auto start = std::chrono::steady_clock::now();
    while (received_count_resp.load(std::memory_order_acquire) < 1) {
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(2)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_GE(received_count_resp.load(), 1) << "Server did not receive array";
    auto &node = buf_resp[0];
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->to_resp(), sent);

    close(fd);
}

TEST_F(RESPTest, ParseNullArray) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(fd, 0) << "socket() failed";

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = PORT;
    addr.sin_addr.s_addr = IP;

    int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    ASSERT_EQ(rv, 0) << "connect() failed";

    // handshake
    write_view(fd, "HELLO 3\r\n", sizeof("HELLO 3\r\n") - 1);
    std::string hresp = read_n(fd, 5);
    ASSERT_EQ(hresp, "+OK\r\n") << "Handshake failed: " << hresp;

    const std::string sent = "*-1\r\n";
    write_view(fd, sent, sent.size());

    std::string ack = read_n(fd, 5);
    ASSERT_EQ(ack, "+OK\r\n") << "Server did not ack array: " << ack;

    const auto start = std::chrono::steady_clock::now();
    while (received_count_resp.load(std::memory_order_acquire) < 1) {
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(2)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_GE(received_count_resp.load(), 1) << "Server did not receive array";
    auto &node = buf_resp[0];
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->to_resp(), sent);

    close(fd);
}

TEST_F(RESPTest, StressRESP) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(fd, 0) << "socket() failed";

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = PORT;
    addr.sin_addr.s_addr = IP;

    int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    ASSERT_EQ(rv, 0) << "connect() failed";

    // handshake
    write_view(fd, "HELLO 3\r\n", sizeof("HELLO 3\r\n") - 1);
    std::string hresp = read_n(fd, 5);
    ASSERT_EQ(hresp, "+OK\r\n") << "Handshake failed: " << hresp;

    std::string sent[500];
    srand(time(NULL));
    for (int i = 0 ; i < 500 ; ++i) {
        sent[i] = generate_random(8);
        write_view(fd, sent[i], sent[i].size());
        std::string ack = read_n(fd, 5);
        ASSERT_EQ(ack, "+OK\r\n") << "Server did not ack array: " << ack;
    }

    const auto start = std::chrono::steady_clock::now();
    while (received_count_resp.load(std::memory_order_acquire) < 1) {
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(2)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_GE(received_count_resp.load(), 1) << "Server did not receive array";
    for (int i = 0 ; i < 500 ; ++i) {
        auto &node = buf_resp[i];
        ASSERT_NE(node, nullptr);
        EXPECT_EQ(node->to_resp(), sent[i]);
    }

    close(fd);
}