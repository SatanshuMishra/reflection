// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "mdns/NativeMdnsAdvertiser.h"
#include "utilities/Logger.h"
#include "utilities/Constants.h"

// Platform headers for socket creation
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

// mjansson/mdns.h — header-only mDNS library
// Must define MDNS_IMPLEMENTATION in exactly one translation unit
#define MDNS_IMPLEMENTATION
#include "mdns.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <climits>
#include <cstring>
#include <vector>

// Link dependencies are managed by CMakeLists.txt: ws2_32, iphlpapi

namespace reflection {

namespace {

constexpr int kSteadyStateIntervalSec = 60;
constexpr size_t kMdnsBufferSize = 2048;

/// Enumerate active non-loopback IPv4 addresses using GetAdaptersAddresses.
std::vector<std::string> get_local_ipv4_addresses() {
    std::vector<std::string> addresses;

    ULONG buf_size = 15000;
    std::vector<uint8_t> buffer(buf_size);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST
                | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;

    ULONG result = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &buf_size);
    if (result == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(buf_size);
        adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        result = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &buf_size);
    }

    if (result != NO_ERROR) {
        Logger::error("GetAdaptersAddresses failed: {}", result);
        return addresses;
    }

    for (auto* adapter = adapters; adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp) continue;
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

        for (auto* addr = adapter->FirstUnicastAddress; addr; addr = addr->Next) {
            if (addr->Address.lpSockaddr->sa_family != AF_INET) continue;

            auto* sin = reinterpret_cast<sockaddr_in*>(addr->Address.lpSockaddr);
            char ip_str[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &sin->sin_addr, ip_str, sizeof(ip_str));

            if (std::string(ip_str) != "0.0.0.0") {
                addresses.emplace_back(ip_str);
            }
        }
    }

    return addresses;
}

/// Create a UDP socket bound to a specific local address for mDNS.
SOCKET create_mdns_socket(const std::string& bind_address) {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        Logger::error("Failed to create mDNS socket: {}", WSAGetLastError());
        return INVALID_SOCKET;
    }

    // Allow address reuse (multiple mDNS responders on same machine)
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    // Bind to mDNS port on the specific interface
    sockaddr_in bind_addr = {};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(MDNS_PORT);
    inet_pton(AF_INET, bind_address.c_str(), &bind_addr.sin_addr);

    if (bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == SOCKET_ERROR) {
        Logger::warn("Failed to bind mDNS socket to {}: {} (will try INADDR_ANY)",
                     bind_address, WSAGetLastError());
        // Fallback: bind to any address
        bind_addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == SOCKET_ERROR) {
            Logger::error("Failed to bind mDNS socket: {}", WSAGetLastError());
            closesocket(sock);
            return INVALID_SOCKET;
        }
    }

    // Join mDNS multicast group 224.0.0.251 on this interface
    ip_mreq mreq = {};
    inet_pton(AF_INET, "224.0.0.251", &mreq.imr_multiaddr);
    inet_pton(AF_INET, bind_address.c_str(), &mreq.imr_interface);

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR) {
        Logger::warn("Failed to join mDNS multicast on {}: {}",
                     bind_address, WSAGetLastError());
        // Continue anyway — unicast announcements may still work
    }

    // Set multicast TTL to 255 (required by mDNS spec)
    int ttl = 255;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
               reinterpret_cast<const char*>(&ttl), sizeof(ttl));

    // Set outgoing multicast interface
    in_addr iface_addr = {};
    inet_pton(AF_INET, bind_address.c_str(), &iface_addr);
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,
               reinterpret_cast<const char*>(&iface_addr), sizeof(iface_addr));

    Logger::info("mDNS socket created on interface {}", bind_address);
    return sock;
}

/// Build mdns_record_t structs from our MdnsServiceRecord for announcement.
/// Returns a vector of additional records (A, SRV, TXT) to send alongside PTR.
struct MdnsRecordSet {
    mdns_record_t ptr_record;
    mdns_record_t srv_record;
    mdns_record_t a_record;
    std::vector<mdns_record_t> txt_records;

    // String storage — mdns_record_t uses non-owning pointers
    std::string service_type_str;
    std::string instance_name_str;
    std::string hostname_str;
    std::vector<std::pair<std::string, std::string>> txt_storage;
};

MdnsRecordSet build_record_set(
    const MdnsServiceRecord& record,
    const sockaddr_in& local_addr
) {
    MdnsRecordSet set;

    // Store strings (mdns_record_t uses non-owning pointers)
    set.service_type_str = record.service_type;
    set.instance_name_str = record.service_name + "." + record.service_type;
    set.hostname_str = record.hostname;

    // PTR record: service_type -> instance_name
    set.ptr_record = {};
    set.ptr_record.name.str = set.service_type_str.c_str();
    set.ptr_record.name.length = set.service_type_str.size();
    set.ptr_record.type = MDNS_RECORDTYPE_PTR;
    set.ptr_record.data.ptr.name.str = set.instance_name_str.c_str();
    set.ptr_record.data.ptr.name.length = set.instance_name_str.size();
    set.ptr_record.rclass = MDNS_CLASS_IN;
    set.ptr_record.ttl = record.ttl_seconds;

    // SRV record: instance_name -> hostname:port
    set.srv_record = {};
    set.srv_record.name.str = set.instance_name_str.c_str();
    set.srv_record.name.length = set.instance_name_str.size();
    set.srv_record.type = MDNS_RECORDTYPE_SRV;
    set.srv_record.data.srv.name.str = set.hostname_str.c_str();
    set.srv_record.data.srv.name.length = set.hostname_str.size();
    set.srv_record.data.srv.port = record.port;
    set.srv_record.data.srv.priority = 0;
    set.srv_record.data.srv.weight = 0;
    set.srv_record.rclass = MDNS_CLASS_IN | MDNS_CACHE_FLUSH;
    set.srv_record.ttl = record.ttl_seconds;

    // A record: hostname -> IP address
    set.a_record = {};
    set.a_record.name.str = set.hostname_str.c_str();
    set.a_record.name.length = set.hostname_str.size();
    set.a_record.type = MDNS_RECORDTYPE_A;
    set.a_record.data.a.addr = local_addr;
    set.a_record.rclass = MDNS_CLASS_IN | MDNS_CACHE_FLUSH;
    set.a_record.ttl = record.ttl_seconds;

    // TXT records
    set.txt_storage.reserve(record.txt_records.size());
    set.txt_records.reserve(record.txt_records.size());

    for (const auto& txt : record.txt_records) {
        set.txt_storage.emplace_back(txt.key, txt.value);
        const auto& stored = set.txt_storage.back();

        mdns_record_t txt_rec = {};
        txt_rec.name.str = set.instance_name_str.c_str();
        txt_rec.name.length = set.instance_name_str.size();
        txt_rec.type = MDNS_RECORDTYPE_TXT;
        txt_rec.data.txt.key.str = stored.first.c_str();
        txt_rec.data.txt.key.length = stored.first.size();
        txt_rec.data.txt.value.str = stored.second.c_str();
        txt_rec.data.txt.value.length = stored.second.size();
        txt_rec.rclass = MDNS_CLASS_IN | MDNS_CACHE_FLUSH;
        txt_rec.ttl = record.ttl_seconds;

        set.txt_records.push_back(txt_rec);
    }

    return set;
}

} // anonymous namespace

// --------------------------------------------------------------------------
// NativeMdnsAdvertiser implementation
// --------------------------------------------------------------------------

NativeMdnsAdvertiser::NativeMdnsAdvertiser() = default;

NativeMdnsAdvertiser::~NativeMdnsAdvertiser() {
    withdraw_all();
}

bool NativeMdnsAdvertiser::advertise(const MdnsServiceRecord& record) {
    {
        std::lock_guard lock(mutex_);

        // Check for duplicate
        for (const auto& existing : records_) {
            if (existing.service_type == record.service_type) {
                Logger::info("Service {} already advertised, updating",
                             record.service_type);
                return true;
            }
        }

        // Set up sockets on first advertise
        if (sockets_.empty()) {
            if (!setup_sockets()) {
                Logger::error("Failed to set up mDNS sockets");
                // Still track the record — sockets may succeed on re-announce
            }
        }

        records_.push_back(record);

        // Start re-announcement thread if not running (checked under lock
        // to prevent TOCTOU race when two advertise() calls arrive concurrently)
        if (!announce_thread_.joinable()) {
            announce_thread_ = std::jthread([this](std::stop_token token) {
                announce_loop(token);
            });
        }
    }

    // Send one immediate announcement, then the announce_loop handles
    // the RFC 6762 §8.3 initial burst (2 more at 1-second intervals).
    Logger::info("Advertising mDNS service: {} ({})",
                 record.service_name, record.service_type);

    send_announcement(record);
    return true;
}

void NativeMdnsAdvertiser::withdraw(const std::string& service_type) {
    MdnsServiceRecord removed_record;
    bool found = false;

    {
        std::lock_guard lock(mutex_);
        auto it = std::find_if(records_.begin(), records_.end(),
            [&](const MdnsServiceRecord& r) {
                return r.service_type == service_type;
            });

        if (it == records_.end()) return;

        removed_record = *it;
        found = true;
        records_.erase(it);
    }

    if (found) {
        send_goodbye(removed_record);
        Logger::info("Withdrew mDNS service: {}", service_type);
    }

    // Stop thread if no more records
    std::lock_guard lock(mutex_);
    if (records_.empty()) {
        // Request thread stop — must release mutex before join
        if (announce_thread_.joinable()) {
            announce_thread_.request_stop();
        }
    }
}

void NativeMdnsAdvertiser::force_reannounce() {
    std::vector<MdnsServiceRecord> snapshot;
    {
        std::lock_guard lock(mutex_);
        snapshot = records_;
    }

    if (snapshot.empty()) return;

    Logger::info("Force re-announcing {} mDNS services", snapshot.size());
    for (const auto& record : snapshot) {
        send_announcement(record);
    }
}

void NativeMdnsAdvertiser::withdraw_all() {
    std::vector<MdnsServiceRecord> snapshot;
    {
        std::lock_guard lock(mutex_);
        snapshot = records_;
        records_.clear();
    }

    // Send goodbye for all records (outside lock)
    for (const auto& record : snapshot) {
        send_goodbye(record);
    }

    // Stop announcement thread (must be outside lock — thread needs mutex)
    if (announce_thread_.joinable()) {
        announce_thread_.request_stop();
        announce_thread_.join();
    }

    // Close sockets
    {
        std::lock_guard lock(mutex_);
        close_sockets();
    }

    if (!snapshot.empty()) {
        Logger::info("Withdrew all {} mDNS services", snapshot.size());
    }
}

bool NativeMdnsAdvertiser::is_advertising() const {
    std::lock_guard lock(mutex_);
    return !records_.empty();
}

std::vector<std::string> NativeMdnsAdvertiser::advertised_services() const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> services;
    services.reserve(records_.size());
    for (const auto& record : records_) {
        services.push_back(record.service_type);
    }
    return services;
}

bool NativeMdnsAdvertiser::setup_sockets() {
    const auto addresses = get_local_ipv4_addresses();
    if (addresses.empty()) {
        Logger::warn("No active non-loopback network interfaces found");
        return false;
    }

    for (const auto& addr : addresses) {
        SOCKET sock = create_mdns_socket(addr);
        if (sock != INVALID_SOCKET) {
            sockets_.push_back(static_cast<uintptr_t>(sock));
        }
    }

    Logger::info("Created {} mDNS sockets across {} interfaces",
                 sockets_.size(), addresses.size());
    return !sockets_.empty();
}

void NativeMdnsAdvertiser::close_sockets() {
    for (auto sock_handle : sockets_) {
        closesocket(static_cast<SOCKET>(sock_handle));
    }
    sockets_.clear();
}

void NativeMdnsAdvertiser::send_announcement(const MdnsServiceRecord& record) {
    std::vector<uintptr_t> socket_snapshot;
    {
        std::lock_guard lock(mutex_);
        socket_snapshot = sockets_;
    }

    if (socket_snapshot.empty()) {
        Logger::debug("No mDNS sockets available for announcement");
        return;
    }

    // Build record set for each socket (each has a different local address)
    std::vector<uint8_t> buffer(kMdnsBufferSize);

    for (auto sock_handle : socket_snapshot) {
        // mdns.h uses int for socket fds (POSIX heritage).
        // On Win64, SOCKET is UINT_PTR (64-bit). In practice, socket values
        // are small, but verify to prevent silent truncation.
        if (sock_handle > static_cast<uintptr_t>(INT_MAX)) {
            Logger::error("Socket handle {} exceeds int range — skipping", sock_handle);
            continue;
        }
        int sock = static_cast<int>(sock_handle);

        // Get the local address bound to this socket
        sockaddr_in local_addr = {};
        int addr_len = sizeof(local_addr);
        getsockname(static_cast<SOCKET>(sock_handle),
                     reinterpret_cast<sockaddr*>(&local_addr), &addr_len);

        auto record_set = build_record_set(record, local_addr);

        // Build additional records array: SRV + A + all TXT
        std::vector<mdns_record_t> additional;
        additional.push_back(record_set.srv_record);
        additional.push_back(record_set.a_record);
        for (const auto& txt : record_set.txt_records) {
            additional.push_back(txt);
        }

        // Send PTR as answer, with SRV + A + TXT as additional
        int result = mdns_announce_multicast(
            sock,
            buffer.data(),
            buffer.size(),
            record_set.ptr_record,
            nullptr, 0,  // no authority records
            additional.data(),
            additional.size()
        );

        if (result < 0) {
            Logger::warn("Failed to send mDNS announcement on socket {}: {}",
                         sock, WSAGetLastError());
        }
    }
}

void NativeMdnsAdvertiser::send_goodbye(const MdnsServiceRecord& record) {
    std::vector<uintptr_t> socket_snapshot;
    {
        std::lock_guard lock(mutex_);
        socket_snapshot = sockets_;
    }

    if (socket_snapshot.empty()) return;

    std::vector<uint8_t> buffer(kMdnsBufferSize);

    for (auto sock_handle : socket_snapshot) {
        if (sock_handle > static_cast<uintptr_t>(INT_MAX)) continue;
        int sock = static_cast<int>(sock_handle);

        sockaddr_in local_addr = {};
        int addr_len = sizeof(local_addr);
        getsockname(static_cast<SOCKET>(sock_handle),
                     reinterpret_cast<sockaddr*>(&local_addr), &addr_len);

        auto record_set = build_record_set(record, local_addr);

        std::vector<mdns_record_t> additional;
        additional.push_back(record_set.srv_record);
        additional.push_back(record_set.a_record);

        int result = mdns_goodbye_multicast(
            sock,
            buffer.data(),
            buffer.size(),
            record_set.ptr_record,
            nullptr, 0,
            additional.data(),
            additional.size()
        );

        if (result < 0) {
            Logger::debug("Failed to send mDNS goodbye on socket {}", sock);
        }
    }
}

void NativeMdnsAdvertiser::announce_loop(std::stop_token stop_token) {
    Logger::info("mDNS announcement thread started");

    // Exponential backoff announcement schedule.
    // iPads discover AirPlay services via mDNS multicast queries and
    // unsolicited announcements. Short initial intervals maximize the
    // chance of being discovered quickly, then we back off to reduce
    // network traffic once the service is well-known.
    //
    // Schedule (seconds between announcements):
    //   1, 1, 5, 5, 10, 10, 15, 30, 60, 60, 60, ...
    constexpr int kSchedule[] = { 1, 1, 5, 5, 10, 10, 15, 30 };
    constexpr int kScheduleLen = sizeof(kSchedule) / sizeof(kSchedule[0]);

    int schedule_index = 0;

    while (!stop_token.stop_requested()) {
        // Determine interval for this cycle
        const int interval = (schedule_index < kScheduleLen)
            ? kSchedule[schedule_index]
            : kSteadyStateIntervalSec;

        // Sleep in 1-second increments for responsive shutdown
        for (int i = 0; i < interval && !stop_token.stop_requested(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (stop_token.stop_requested()) break;

        // Snapshot records under lock, send outside lock
        std::vector<MdnsServiceRecord> snapshot;
        {
            std::lock_guard lock(mutex_);
            snapshot = records_;
        }

        for (const auto& record : snapshot) {
            send_announcement(record);
        }

        if (schedule_index < kScheduleLen) {
            Logger::debug("mDNS announcement {}/{} sent ({} services, next in {}s)",
                          schedule_index + 1, kScheduleLen, snapshot.size(),
                          (schedule_index + 1 < kScheduleLen)
                              ? kSchedule[schedule_index + 1]
                              : kSteadyStateIntervalSec);
        } else {
            Logger::debug("Re-announced {} mDNS services", snapshot.size());
        }

        ++schedule_index;
    }

    Logger::info("mDNS announcement thread stopped");
}

} // namespace reflection
