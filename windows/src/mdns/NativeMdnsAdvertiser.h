// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include "mdns/IMdnsAdvertiser.h"

#include <mutex>
#include <thread>
#include <vector>

// Forward declare — mdns.h is included only in the .cpp
struct sockaddr_in;

namespace reflection {

/// Production mDNS advertiser using mjansson/mdns.h directly.
///
/// Creates UDP sockets via Winsock2, joins the mDNS multicast group
/// (224.0.0.251:5353) on each active non-loopback interface, and sends
/// real mDNS announcement packets using mdns_announce_multicast().
///
/// On withdraw, sends mDNS goodbye packets (TTL=0).
/// Periodically re-announces (every 60s) until stopped.
///
/// Unlike MjanssonMdnsAdvertiser, this class does NOT use ISocket/INetworkInterface
/// abstractions — it uses platform APIs directly for real mDNS. The mock-based
/// MjanssonMdnsAdvertiser + tests verify logic flow; this class does the real work.
class NativeMdnsAdvertiser : public IMdnsAdvertiser {
public:
    NativeMdnsAdvertiser();
    ~NativeMdnsAdvertiser() override;

    // Non-copyable, non-movable
    NativeMdnsAdvertiser(const NativeMdnsAdvertiser&) = delete;
    NativeMdnsAdvertiser& operator=(const NativeMdnsAdvertiser&) = delete;

    [[nodiscard]] bool advertise(const MdnsServiceRecord& record) override;
    void withdraw(const std::string& service_type) override;
    void withdraw_all() override;
    void force_reannounce() override;
    [[nodiscard]] bool is_advertising() const override;
    [[nodiscard]] std::vector<std::string> advertised_services() const override;

private:
    mutable std::mutex mutex_;
    std::vector<MdnsServiceRecord> records_;

    // Platform sockets (SOCKET on Windows, int on POSIX)
    // Stored as uintptr_t for cross-platform compatibility
    std::vector<uintptr_t> sockets_;

    std::jthread announce_thread_;

    /// Open sockets on all active non-loopback interfaces and join mDNS multicast.
    bool setup_sockets();

    /// Close all sockets.
    void close_sockets();

    /// Send mDNS announcement packets for a record on all sockets.
    void send_announcement(const MdnsServiceRecord& record);

    /// Send mDNS goodbye (TTL=0) for a record on all sockets.
    void send_goodbye(const MdnsServiceRecord& record);

    /// Background thread: periodically re-announces all records.
    void announce_loop(std::stop_token stop_token);
};

} // namespace reflection
