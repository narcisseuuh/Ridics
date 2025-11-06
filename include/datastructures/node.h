#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <vector>
#include <optional>
#include <utility>

namespace data {

class Node {
public:
    virtual ~Node() = default;
    virtual std::string to_string() const = 0;
    virtual std::string to_resp() const = 0;
};

std::ostream& operator<<(std::ostream& out, const Node& n);

class BulkString : public Node {
public:
    BulkString(std::string s) : _s(s) {}
    BulkString() {}

    std::string to_string() const override {
        if (_s.has_value()) {
            return _s.value();
        } else {
            // null in red (to differenciate from "null")
            return "\033[31;mnull\033[0m";
        }
    }

    const std::optional<std::string>& get() const {
        return _s;
    }

    std::string to_resp() const override {
        if (_s.has_value()) {
            auto& s = _s.value();
            size_t n = s.size();
            return "$" + std::to_string(n) + "\r\n" + s + "\r\n";
        } else {
            return "$-1\r\n";
        }
    }


    ~BulkString() {}
private:
    std::optional<std::string> _s;
};

class String : public Node {
public:
    String(std::string s) : _s(s) {}

    std::string to_string() const override {
        return _s;
    }

    const std::string& get() const {
        return _s;
    }

    std::string to_resp() const override {
        return "+" + _s + "\r\n";
    }

    ~String() {}
private:
    std::string _s;
};

class Integer : public Node {
public:
    Integer(int64_t i) : _i(i) {}

    std::string to_string() const override {
        return std::to_string(_i);
    }

    const int64_t& get() const {
        return _i;
    }

    std::string to_resp() const override {
        return ":" + std::to_string(_i) + "\r\n";
    }

private:
    int64_t _i;
};

class Array : public Node {
public:
    explicit Array(int len) {
        if (len >= 0) {
            _a.emplace();
            _a->reserve(static_cast<size_t>(len));
        }
    }

    std::string to_string() const override {
        if (!_a.has_value()) {
            return "\033[31;mnull\033[0m";
        }
        auto& a = _a.value();
        int n = a.size();
        std::string s = "[";
        for (int i = 0 ; i < n ; ++i) {
            s += a[i]->to_string();
            if (i == n - 1) {
                break;
            }
            s += ", ";
        }
        s += "]";
        return s;
    }

    const std::optional<std::vector<std::unique_ptr<Node>>>& get() const {
        return _a;
    }

    void push_back(std::unique_ptr<Node> n) {
        if (!_a.has_value()) return;
        _a.value().push_back(std::move(n));
    }

    std::string to_resp() const override {
        if (!_a.has_value()) return "*-1\r\n";
        auto& a = _a.value();
        int n = a.size();
        std::string s = "*" + std::to_string(n) + "\r\n";
        for (auto&& rv : a) {
            s += rv->to_resp();
        }
        return s;
    }
    
    ~Array() {}

private:
    std::optional<std::vector<std::unique_ptr<Node>>> _a;
};

} // namespace data