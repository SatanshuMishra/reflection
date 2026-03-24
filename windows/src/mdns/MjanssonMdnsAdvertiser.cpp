// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "mdns/MjanssonMdnsAdvertiser.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

#include <chrono>

namespace reflection {

MjanssonMdnsAdvertiser::MjanssonMdnsAdvertiser(
    std::unique_ptr<ISocketFactory> socket_factory,
    std::unique_ptr<INetworkInterface> network_interface)
    : socket_factory_(std::move(socket_factory))
    , network_interface_(std::move(network_interface))
{
}

MjanssonMdnsAdvertiser::~MjanssonMdnsAdvertiser() {
    withdraw_all();
}

bool MjanssonMdnsAdvertiser::advertise(const MdnsServiceRecord& record) {
    std::lock_guard lock(mutex_);

    // Set up sockets if not already done
    if (sockets_.empty()) {
        if (!setup_sockets()) {
            Logger::error("MdnsAdvertiser: Failed to set up sockets");
            return false;
        }
    }

    records_.push_back(record);
    send_announcement(record);

    // Start re-announcement thread if not already running
    if (!announce_thread_.joinable()) {
        announce_thread_ = std::jthread([this](std::stop_token st) {
            announce_loop(st);
        });
    }

    Logger::info("MdnsAdvertiser: Advertising '{}' on {}", record.service_name, record.service_type);
    return true;
}

void MjanssonMdnsAdvertiser::withdraw(const std::string& service_type) {
    bool should_stop_thread = false;

    {
        std::lock_guard lock(mutex_);

        // Send goodbye for matching records
        for (const auto& record : records_) {
            if (record.service_type == service_type) {
                send_goodbye(record);
            }
        }

        // Remove from active records
        std::erase_if(records_, [&](const MdnsServiceRecord& r) {
            return r.service_type == service_type;
        });

        should_stop_thread = records_.empty();
    }
    // Mutex released before join — prevents deadlock with announce_loop.

    if (should_stop_thread) {
        if (announce_thread_.joinable()) {
            announce_thread_.request_stop();
            announce_thread_.join();
        }
        std::lock_guard lock(mutex_);
        close_sockets();
    }
}

void MjanssonMdnsAdvertiser::force_reannounce() {
    std::lock_guard lock(mutex_);
    for (const auto& record : records_) {
        send_announcement(record);
    }
}

void MjanssonMdnsAdvertiser::withdraw_all() {
    {
        std::lock_guard lock(mutex_);

        // Send goodbye for all records
        for (const auto& record : records_) {
            send_goodbye(record);
        }
        records_.clear();
    }
    // Mutex released before join — prevents deadlock with announce_loop.

    if (announce_thread_.joinable()) {
        announce_thread_.request_stop();
        announce_thread_.join();
    }

    std::lock_guard lock(mutex_);
    close_sockets();
}

bool MjanssonMdnsAdvertiser::is_advertising() const {
    std::lock_guard lock(mutex_);
    return !records_.empty();
}

std::vector<std::string> MjanssonMdnsAdvertiser::advertised_services() const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> result;
    result.reserve(records_.size());
    for (const auto& r : records_) {
        result.push_back(r.service_type);
    }
    return result;
}

bool MjanssonMdnsAdvertiser::setup_sockets() {
    const auto interfaces = network_interface_->list_interfaces();
    if (interfaces.empty()) {
        Logger::warn("MdnsAdvertiser: No network interfaces found");
        return false;
    }

    bool any_success = false;
    for (const auto& iface : interfaces) {
        if (iface.is_loopback || !iface.is_up || iface.ipv4_address.empty()) {
            continue;
        }

        auto socket = socket_factory_->create_udp();
        if (!socket) continue;

        auto bind_result = socket->bind(iface.ipv4_address, constants::kMdnsPort);
        if (!bind_result.success) {
            Logger::warn("MdnsAdvertiser: Failed to bind on {}: {}",
                         iface.ipv4_address, bind_result.error_message);
            continue;
        }

        auto join_result = socket->join_multicast(
            std::string(constants::kMdnsMulticastAddress),
            iface.ipv4_address);
        if (!join_result.success) {
            Logger::warn("MdnsAdvertiser: Failed to join multicast on {}: {}",
                         iface.ipv4_address, join_result.error_message);
            socket->close();
            continue;
        }

        Logger::debug("MdnsAdvertiser: Socket ready on {}", iface.ipv4_address);
        sockets_.push_back(std::move(socket));
        any_success = true;
    }

    return any_success;
}

void MjanssonMdnsAdvertiser::send_announcement(const MdnsServiceRecord& record) {
    // TODO: Build proper mDNS response packet using mjansson/mdns.h
    // The actual packet construction will use mdns.h functions to build
    // PTR + SRV + TXT + A records conforming to RFC 6762.
    // Until then, log only — do NOT send malformed packets to the network.
    Logger::debug("MdnsAdvertiser: send_announcement for '{}' ({}) port {} on {} socket(s)",
                  record.service_name, record.service_type, record.port, sockets_.size());
}

void MjanssonMdnsAdvertiser::send_goodbye(const MdnsServiceRecord& record) {
    // TODO: Build mDNS goodbye packet (same as announcement but TTL=0)
    // Until implemented, log only.
    Logger::debug("MdnsAdvertiser: send_goodbye for '{}' ({})",
                  record.service_name, record.service_type);
}

void MjanssonMdnsAdvertiser::announce_loop(std::stop_token stop_token) {
    using namespace std::chrono;
    constexpr int kReannounceIntervalSec = 60;

    while (!stop_token.stop_requested()) {
        // Sleep in 1-second increments for responsive shutdown.
        // Re-announce every kReannounceIntervalSec seconds.
        for (int i = 0; i < kReannounceIntervalSec && !stop_token.stop_requested(); ++i) {
            std::this_thread::sleep_for(seconds(1));
        }

        if (stop_token.stop_requested()) break;

        // Snapshot records under lock, then release before doing I/O.
        // This prevents deadlock with withdraw/withdraw_all which also
        // acquire mutex_ and then call join() on this thread.
        std::vector<MdnsServiceRecord> snapshot;
        {
            std::lock_guard lock(mutex_);
            snapshot = records_;
        }

        for (const auto& record : snapshot) {
            send_announcement(record);
        }
    }
}

void MjanssonMdnsAdvertiser::close_sockets() {
    for (auto& socket : sockets_) {
        socket->close();
    }
    sockets_.clear();
}

} // namespace reflection
