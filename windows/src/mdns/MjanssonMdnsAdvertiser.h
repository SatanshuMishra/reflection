#pragma once

#include "mdns/IMdnsAdvertiser.h"
#include "network/INetworkInterface.h"
#include "network/ISocket.h"

#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace reflection {

/// mDNS advertiser using mjansson/mdns.h (header-only library).
///
/// Advertises services on all active non-loopback network interfaces
/// by joining the mDNS multicast group (224.0.0.251:5353) and
/// periodically re-announcing service records.
///
/// Dependencies injected for testability:
/// - ISocketFactory: creates UDP sockets
/// - INetworkInterface: enumerates local network interfaces
class MjanssonMdnsAdvertiser : public IMdnsAdvertiser {
public:
    MjanssonMdnsAdvertiser(
        std::unique_ptr<ISocketFactory> socket_factory,
        std::unique_ptr<INetworkInterface> network_interface);

    ~MjanssonMdnsAdvertiser() override;

    [[nodiscard]] bool advertise(const MdnsServiceRecord& record) override;
    void withdraw(const std::string& service_type) override;
    void withdraw_all() override;
    void force_reannounce() override;
    [[nodiscard]] bool is_advertising() const override;
    [[nodiscard]] std::vector<std::string> advertised_services() const override;

private:
    std::unique_ptr<ISocketFactory> socket_factory_;
    std::unique_ptr<INetworkInterface> network_interface_;

    mutable std::mutex mutex_;
    std::vector<MdnsServiceRecord> records_;
    std::vector<std::unique_ptr<ISocket>> sockets_;
    std::jthread announce_thread_;

    /// Create sockets for all active non-loopback interfaces.
    bool setup_sockets();

    /// Send mDNS announcement for a record on all sockets.
    void send_announcement(const MdnsServiceRecord& record);

    /// Send mDNS goodbye (TTL=0) for a record on all sockets.
    void send_goodbye(const MdnsServiceRecord& record);

    /// Periodic re-announcement loop (runs on announce_thread_).
    void announce_loop(std::stop_token stop_token);

    /// Close all sockets.
    void close_sockets();
};

} // namespace reflection
