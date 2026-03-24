#pragma once

#include "mdns/MdnsServiceRecord.h"

#include <vector>

namespace reflection {

/// Abstract interface for mDNS service advertisement.
/// Wraps mjansson/mdns.h or any other mDNS implementation.
class IMdnsAdvertiser {
public:
    virtual ~IMdnsAdvertiser() = default;

    /// Advertise the given service record on the local network.
    /// Returns true if advertisement was successfully started.
    [[nodiscard]] virtual bool advertise(const MdnsServiceRecord& record) = 0;

    /// Withdraw a previously advertised service (sends goodbye with TTL=0).
    virtual void withdraw(const std::string& service_type) = 0;

    /// Withdraw all advertised services.
    virtual void withdraw_all() = 0;

    /// Force an immediate mDNS re-announcement burst for all services.
    /// Use after disconnect/reconnect to ensure iPads rediscover quickly.
    virtual void force_reannounce() = 0;

    /// Whether any services are currently being advertised.
    [[nodiscard]] virtual bool is_advertising() const = 0;

    /// Get the list of currently advertised service types.
    [[nodiscard]] virtual std::vector<std::string> advertised_services() const = 0;
};

} // namespace reflection
