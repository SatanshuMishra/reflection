#pragma once

#include "network/INetworkInterface.h"

namespace reflection::testing {

/// Mock network interface enumerator for unit tests.
class MockNetworkInterface : public INetworkInterface {
public:
    std::vector<NetworkInterfaceInfo> interfaces;
    mutable int list_call_count = 0;

    MockNetworkInterface() {
        // Default: one active non-loopback interface
        interfaces.push_back({
            "eth0", "192.168.1.100", "fe80::1", false, true
        });
    }

    [[nodiscard]] std::vector<NetworkInterfaceInfo> list_interfaces() const override {
        ++list_call_count;
        return interfaces;
    }

    /// Configure to return no interfaces (simulates disconnected network).
    void set_no_interfaces() { interfaces.clear(); }

    /// Configure to return only loopback.
    void set_loopback_only() {
        interfaces.clear();
        interfaces.push_back({"lo", "127.0.0.1", "::1", true, true});
    }

    /// Add a second interface.
    void add_interface(const std::string& name, const std::string& ipv4) {
        interfaces.push_back({name, ipv4, "", false, true});
    }
};

} // namespace reflection::testing
