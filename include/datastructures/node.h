#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>

namespace data {

class Node {
public:
    virtual ~Node() = default;
    virtual std::string to_string() const = 0;
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

private:
    int64_t _i;
};

class Array : public Node {
public:
    explicit Array(size_t len) {
        _a.reserve(len);
    }

    std::string to_string() const override {
        int n = _a.size();
        std::string s = "[";
        for (int i = 0 ; i < n ; ++i) {
            s += _a[i]->to_string();
            if (i == n - 1) {
                break;
            }
            s += ", ";
        }
        s += "]";
        return s;
    }

    const std::vector<std::unique_ptr<Node>>& get() const {
        return _a;
    }

    void push_back(std::unique_ptr<Node> n) {
        _a.push_back(std::move(n));
    }
    
    ~Array() {}

private:
    std::vector<std::unique_ptr<Node>> _a;
};

} // namespace data