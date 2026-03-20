#pragma once

#include <string>
#include <vector>

namespace reflection {

/// Information about a network interface on the local machine.
struct NetworkInterfaceInfo {
    std::string name;
    std::string ipv4_address;
    std::string ipv6_address;
    bool is_loopback = false;
    bool is_up = false;

    bool operator==(const NetworkInterfaceInfo&) const = default;
};

/// Platform-agnostic interface for enumerating local network interfaces.
/// Abstracts GetAdaptersAddresses (Windows) vs getifaddrs (POSIX).
class INetworkInterface {
public:
    virtual ~INetworkInterface() = default;

    /// List all active, non-loopback network interfaces.
    [[nodiscard]] virtual std::vector<NetworkInterfaceInfo> list_interfaces() const = 0;
};

} // namespace reflection
