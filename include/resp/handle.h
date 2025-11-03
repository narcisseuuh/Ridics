#include "resp/server.h"
#include <list>

struct ChainOfResponsibility {
    template<typename... Args>
    struct Chain {
        template<typename Callee, typename Next>
        Chain(const Callee c, const Next& n) {
            m_impl = c;
            m_next = n;
        }

        template<typename Callee>
        decltype(auto) attach(Callee c) {
            return Chain(c, *this);
        }

        void operator()(Args... e) {
            m_impl(e..., m_next);
        }

        std::function<void(Args..., std::function<void(Args...)>)> m_impl;
        std::function<void(Args...)> m_next;
    };
 
    template<typename... Args>
    struct ChainTail
    {
        template<typename Callee>
        ChainTail(Callee c) {
            m_impl = c;
        }
        
        template<typename Callee>
        decltype(auto) attach(Callee c) {
            return Chain<Args...>(c, m_impl);
        }

        void operator()(Args... e) {
            m_impl(e...);
        }

        std::function<void(Args... e)> m_impl;
    };

    template<typename>
    struct StartChain;  
    
    template<typename C, typename... Args>
    struct StartChain<void (C::*)(Args...) const> {
        using Type = ChainTail<Args...>;
    };

    template<typename Callee>
    static decltype(auto) start_new(Callee c) {
        return StartChain<decltype(&Callee::operator())>::Type(c);
    }
};

namespace net {

namespace resp {

class Redis {
public:
    using T = std::variant<net::resp::RESPError, std::unique_ptr<data::Node>>;

    Redis(unsigned int s_addr, unsigned short port, const int k_max_msg)
        : _s(s_addr, port, k_max_msg),
          _chain(std::make_unique<ChainOfResponsibility::ChainTail<int, T&&>>(
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
    std::unique_ptr<ChainOfResponsibility::ChainTail<int, T&&>> _chain;
};

}

}