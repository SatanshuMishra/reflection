#include "airplay/AirPlayTypes.h"

#include <iomanip>
#include <sstream>

namespace reflection {

std::string AirPlayServiceConfig::hw_address_hex() const {
    std::ostringstream oss;
    for (size_t i = 0; i < hardware_address.size(); ++i) {
        if (i > 0) oss << ':';
        oss << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<int>(hardware_address[i]);
    }
    return oss.str();
}

} // namespace reflection
