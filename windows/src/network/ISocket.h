// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace reflection {

/// Result of a socket operation.
struct SocketResult {
    bool success = false;
    int error_code = 0;
    std::string error_message;

    static SocketResult ok() { return {true, 0, {}}; }

    static SocketResult fail(int code, const std::string& msg) {
        return {false, code, msg};
    }
};

/// Result of a recvfrom operation.
struct RecvResult {
    SocketResult status;
    size_t bytes_received = 0;
    std::string source_address;
    uint16_t source_port = 0;
};

/// Platform-agnostic UDP socket interface.
/// Abstracts BSD sockets vs Winsock2 for testability.
class ISocket {
public:
    virtual ~ISocket() = default;

    /// Bind to the given address and port.
    [[nodiscard]] virtual SocketResult bind(
        const std::string& address, uint16_t port) = 0;

    /// Send data to the specified address and port.
    [[nodiscard]] virtual SocketResult sendto(
        const uint8_t* data, size_t size,
        const std::string& address, uint16_t port) = 0;

    /// Receive data with timeout. Returns when data arrives or timeout expires.
    [[nodiscard]] virtual RecvResult recvfrom(
        uint8_t* buffer, size_t buffer_size, uint32_t timeout_ms) = 0;

    /// Join a multicast group on the specified local interface.
    [[nodiscard]] virtual SocketResult join_multicast(
        const std::string& group_address,
        const std::string& interface_address) = 0;

    /// Close the socket.
    virtual void close() = 0;

    /// Whether the socket is open.
    [[nodiscard]] virtual bool is_open() const = 0;
};

/// Factory for creating platform-appropriate sockets.
class ISocketFactory {
public:
    virtual ~ISocketFactory() = default;

    /// Create a new UDP socket.
    [[nodiscard]] virtual std::unique_ptr<ISocket> create_udp() = 0;
};

} // namespace reflection
