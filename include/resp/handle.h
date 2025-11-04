#include "resp/server.h"
#include <list>

#include <functional>
#include <utility>

struct ChainOfResponsibility {
    template<typename... Args>
    struct Chain {
        using NextFn = std::function<void(Args...)>;
        using HandlerFn = std::function<void(Args..., NextFn)>;

        Chain(HandlerFn h, NextFn n) : handler(std::move(h)), next(std::move(n)) {}

        explicit Chain(NextFn n) : handler(nullptr), next(std::move(n)) {}

        template<typename Callee>
        static HandlerFn wrap_callee(Callee c) {
            return [c = std::move(c)](Args... args, NextFn next) {
                Chain proxy(next);
                c(std::forward<Args>(args)..., proxy);
            };
        }

        template<typename Callee>
        Chain attach(Callee c) const {
            return Chain(wrap_callee(std::move(c)), next);
        }

        void operator()(Args... args) const {
            if (handler) {
                handler(std::forward<Args>(args)..., next);
            } else if (next) {
                next(std::forward<Args>(args)...);
            }
        }

        HandlerFn handler;
        NextFn next;
    };

    template<typename... Args>
    struct ChainTail {
        using NextFn = std::function<void(Args...)>;

        explicit ChainTail(NextFn n) : next(std::move(n)) {}

        template<typename Callee>
        auto attach(Callee c) const {
            return Chain<Args...>(Chain<Args...>::wrap_callee(std::move(c)), next);
        }

        void operator()(Args... args) const {
            if (next) next(std::forward<Args>(args)...);
        }

        NextFn next;
    };
};

namespace net {

namespace resp {

class Redis {
public:
    using T = std::variant<net::resp::RESPError, std::unique_ptr<data::Node>>;

    Redis(unsigned int s_addr, unsigned short port, const int k_max_msg)
        : _s(s_addr, port, k_max_msg),
          _chain(std::make_unique<ChainOfResponsibility::Chain<int, T&&>>(
              [](int connfd, T&& n) {
                  std::string err_msg = "-ERR " +
                      std::get<net::resp::RESPError>(n).to_string() + "\r\n";
                  net::write_stream(connfd, err_msg.c_str(), err_msg.size());
              }))
    {}

    void attach(std::function<void(int, T&&, ChainOfResponsibility::Chain<int, T&&>)> c) {
        _chain = std::move(std::make_unique<ChainOfResponsibility::Chain<int, T&&>>(
            _chain->attach(c)
        ));
    }

    void accept(int n) {
        auto* chain = _chain.get();
        _s.tcp_accept([chain] (int connfd, T&& msg) {
            (*chain)(connfd, std::move(msg));
        }, n);
    }

    void accept_all() {
        auto* chain = _chain.get();
        _s.tcp_accept_all([chain] (int connfd, T&& msg) {
            (*chain)(connfd, std::move(msg));
        });
    }

private:
    net::resp::RESPServer _s;
    std::unique_ptr<ChainOfResponsibility::Chain<int, T&&>> _chain;
};

}

}