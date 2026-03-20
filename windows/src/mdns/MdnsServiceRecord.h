#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace reflection {

/// A key-value pair in an mDNS TXT record.
struct MdnsTxtEntry {
    std::string key;
    std::string value;

    bool operator==(const MdnsTxtEntry&) const = default;
};

/// Immutable mDNS service record for advertisement.
struct MdnsServiceRecord {
    std::string service_name;
    std::string service_type;     // e.g., "_airplay._tcp.local."
    std::string hostname;         // e.g., "Reflection.local."
    uint16_t port = 0;
    uint32_t ttl_seconds = 4500;  // mDNS default
    std::vector<MdnsTxtEntry> txt_records;

    bool operator==(const MdnsServiceRecord&) const = default;

    /// Create an AirPlay service record with standard TXT keys.
    /// @param server_name  The name visible on the iPad (e.g., "Reflection")
    /// @param port         AirPlay HTTP port (default 7000)
    /// @param hw_addr_hex  Hardware address as hex string (e.g., "AA:BB:CC:DD:EE:FF")
    static MdnsServiceRecord make_airplay_record(
        const std::string& server_name,
        uint16_t port,
        const std::string& hw_addr_hex);

    /// Create a RAOP service record with standard TXT keys.
    /// @param server_name  The name visible on the iPad
    /// @param port         RAOP (RTSP) port (default 5000)
    /// @param hw_addr_hex  Hardware address as hex string
    static MdnsServiceRecord make_raop_record(
        const std::string& server_name,
        uint16_t port,
        const std::string& hw_addr_hex);
};

} // namespace reflection
