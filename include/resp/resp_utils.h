#include "datastructures/node.h"

namespace net {

namespace resp {

static inline std::string ok() {
    return "+OK\r\n";
}

static inline std::string err(std::string& s) {
    return "-ERR " + s + "\r\n";
}

} // namespace resp

} // namespace net