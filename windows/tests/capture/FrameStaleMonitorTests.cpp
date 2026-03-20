#include <gtest/gtest.h>

#include "session/FrameStaleMonitor.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace reflection::testing {

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

    // Wait for the monitor to detect the frame
    for (int i = 0; i < 20 && !status_received.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_TRUE(status_received.load());
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

    // Wait for receiving status
    for (int i = 0; i < 20 && status_count.load() < 1; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT_TRUE(last_status.load());

    // Now stop recording frames and wait for stale detection
    for (int i = 0; i < 40 && status_count.load() < 2; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

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

        for (int i = 0; i < 20 && !received_status.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        EXPECT_TRUE(received_status.load());
        monitor.stop_monitoring();
    }
}

} // namespace reflection::testing
