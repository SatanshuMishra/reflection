// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include <gtest/gtest.h>

#include "session/FrameStaleMonitor.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace reflection::testing {
namespace {

bool wait_until(const std::function<bool()>& predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

} // namespace

TEST(FrameStaleMonitorTest, InitiallyNotReceiving) {
    FrameStaleMonitor monitor;
    EXPECT_FALSE(monitor.is_receiving_frames());
}

TEST(FrameStaleMonitorTest, RecordFrameThenBecomesReceiving) {
    // Use short intervals for fast tests
    FrameStaleMonitor monitor(0.2, 50); // 200ms threshold, 50ms check

    std::atomic<bool> status_received{false};
    std::atomic<bool> last_status{false};

    monitor.start_monitoring([&](bool receiving) {
        last_status.store(receiving);
        status_received.store(true);
    });

    // Record a frame
    monitor.record_frame();

    // Wait up to 2 seconds for the monitor callback.
    EXPECT_TRUE(wait_until([&] { return status_received.load(); }, std::chrono::milliseconds(2000)));
    EXPECT_TRUE(last_status.load());
    EXPECT_TRUE(monitor.is_receiving_frames());
}

TEST(FrameStaleMonitorTest, BecomesStaleWhenFramesStop) {
    FrameStaleMonitor monitor(0.1, 50); // 100ms threshold, 50ms check

    std::atomic<int> status_count{0};
    std::atomic<bool> last_status{false};

    monitor.start_monitoring([&](bool receiving) {
        last_status.store(receiving);
        status_count.fetch_add(1);
    });

    // Record a frame so it becomes "receiving"
    monitor.record_frame();

    // Wait for receiving status (generous timeout for slow CI runners).
    EXPECT_TRUE(wait_until([&] { return status_count.load() >= 1; }, std::chrono::milliseconds(2500)));
    EXPECT_TRUE(last_status.load());

    // Now stop recording frames and wait for stale detection.
    EXPECT_TRUE(wait_until([&] { return status_count.load() >= 2; }, std::chrono::milliseconds(4500)));

    EXPECT_FALSE(last_status.load());
    EXPECT_FALSE(monitor.is_receiving_frames());
}

TEST(FrameStaleMonitorTest, StopMonitoringResetsState) {
    FrameStaleMonitor monitor(0.2, 50);

    std::atomic<bool> called{false};

    monitor.start_monitoring([&](bool receiving) {
        if (!receiving) called.store(true);
    });

    monitor.record_frame();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    monitor.stop_monitoring();

    EXPECT_FALSE(monitor.is_receiving_frames());
}

TEST(FrameStaleMonitorTest, RestartsCleanly) {
    FrameStaleMonitor monitor(0.2, 50);

    // Start and stop twice
    for (int round = 0; round < 2; ++round) {
        std::atomic<bool> received_status{false};

        monitor.start_monitoring([&](bool /*receiving*/) {
            received_status.store(true);
        });

        monitor.record_frame();

        EXPECT_TRUE(wait_until([&] { return received_status.load(); }, std::chrono::milliseconds(2000)));
        monitor.stop_monitoring();
    }
}

} // namespace reflection::testing
