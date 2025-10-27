#include "http/server.h"
#include <iostream>
#include <functional>

void net::die(const std::string& msg) {
    std::cerr << "failure : " << msg << '\n';
    exit(1);
}