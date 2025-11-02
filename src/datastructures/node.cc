#include "datastructures/node.h"

namespace data {

std::ostream& operator<<(std::ostream& out, const data::Node& n) {
    return out << n.to_string();
}

} // namespace data