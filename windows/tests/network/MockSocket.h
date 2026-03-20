#pragma once

#include "network/ISocket.h"

#include <memory>
#include <vector>

namespace reflection::testing {

/// Mock socket for unit testing mDNS and network code.
class MockSocket : public ISocket {
public:
    // Configurable behavior
    bool should_fail_bind = false;
    bool should_fail_sendto = false;
    bool should_fail_join_multicast = false;
    std::string bind_error_message = "Mock bind failure";

    // Call tracking
    int bind_call_count = 0;
    int sendto_call_count = 0;
    int recvfrom_call_count = 0;
    int join_multicast_call_count = 0;
    int close_call_count = 0;

    // Captured arguments
    std::string last_bind_address;
    uint16_t last_bind_port = 0;
    std::string last_sendto_address;
    uint16_t last_sendto_port = 0;
    std::vector<uint8_t> last_sent_data;
    std::string last_multicast_group;
    std::string last_multicast_interface;

    // State
    bool open_ = true;

    SocketResult bind(const std::string& address, uint16_t port) override {
        ++bind_call_count;
        last_bind_address = address;
        last_bind_port = port;
        if (should_fail_bind) {
            return SocketResult::fail(10048, bind_error_message);
        }
        return SocketResult::ok();
    }

    SocketResult sendto(
        const uint8_t* data, size_t size,
        const std::string& address, uint16_t port
    ) override {
        ++sendto_call_count;
        last_sendto_address = address;
        last_sendto_port = port;
        last_sent_data.assign(data, data + size);
        if (should_fail_sendto) {
            return SocketResult::fail(10065, "Mock sendto failure");
        }
        return SocketResult::ok();
    }

    RecvResult recvfrom(
        uint8_t* /*buffer*/, size_t /*buffer_size*/, uint32_t /*timeout_ms*/
    ) override {
        ++recvfrom_call_count;
        return {SocketResult::fail(10060, "Timeout"), 0, {}, 0};
    }

    SocketResult join_multicast(
        const std::string& group_address,
        const std::string& interface_address
    ) override {
        ++join_multicast_call_count;
        last_multicast_group = group_address;
        last_multicast_interface = interface_address;
        if (should_fail_join_multicast) {
            return SocketResult::fail(10049, "Mock join failure");
        }
        return SocketResult::ok();
    }

    void close() override {
        ++close_call_count;
        open_ = false;
    }

    [[nodiscard]] bool is_open() const override { return open_; }
};

/// Mock socket factory that uses shared_ptr to keep sockets alive
/// for test inspection even after the advertiser destroys its unique_ptr.
///
/// The factory creates sockets as shared_ptr, wraps them in a
/// shared_ptr-backed unique_ptr with a custom deleter. This ensures
/// the MockSocket object stays alive as long as either the factory's
/// vector or the consumer's unique_ptr references it.
class MockSocketFactory : public ISocketFactory {
public:
    // Track created sockets for inspection — safe even after advertiser frees them
    std::vector<std::shared_ptr<MockSocket>> created_sockets;
    bool should_fail_create = false;

    // Pre-configure behavior for all created sockets
    bool sockets_should_fail_bind = false;
    bool sockets_should_fail_join = false;

    std::unique_ptr<ISocket> create_udp() override {
        if (should_fail_create) return nullptr;

        auto sock = std::make_shared<MockSocket>();
        sock->should_fail_bind = sockets_should_fail_bind;
        sock->should_fail_join_multicast = sockets_should_fail_join;
        created_sockets.push_back(sock);

        // Return unique_ptr with custom deleter that releases the shared_ptr
        // The MockSocket stays alive via created_sockets vector
        return std::unique_ptr<ISocket>(new SharedPtrWrapper(sock));
    }

private:
    /// Wrapper that delegates all ISocket calls to the underlying shared_ptr.
    /// When destroyed by the unique_ptr, the MockSocket stays alive via shared_ptr.
    class SharedPtrWrapper : public ISocket {
    public:
        explicit SharedPtrWrapper(std::shared_ptr<MockSocket> inner)
            : inner_(std::move(inner)) {}

        SocketResult bind(const std::string& addr, uint16_t port) override {
            return inner_->bind(addr, port);
        }
        SocketResult sendto(const uint8_t* data, size_t size,
                            const std::string& addr, uint16_t port) override {
            return inner_->sendto(data, size, addr, port);
        }
        RecvResult recvfrom(uint8_t* buf, size_t size, uint32_t timeout) override {
            return inner_->recvfrom(buf, size, timeout);
        }
        SocketResult join_multicast(const std::string& group,
                                     const std::string& iface) override {
            return inner_->join_multicast(group, iface);
        }
        void close() override { inner_->close(); }
        [[nodiscard]] bool is_open() const override { return inner_->is_open(); }

    private:
        std::shared_ptr<MockSocket> inner_;
    };
};

} // namespace reflection::testing
