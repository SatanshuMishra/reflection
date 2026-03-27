// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include <gtest/gtest.h>

#include "session/FrameStaleMonitor.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

using namespace std::chrono_literals;

namespace reflection::testing {

TEST(FrameStaleMonitorTest, InitiallyNotReceiving) {
    FrameStaleMonitor monitor;
    EXPECT_FALSE(monitor.is_receiving_frames());
}

TEST(FrameStaleMonitorTest, RecordFrameThenBecomesReceiving) {
    FrameStaleMonitor monitor(0.2, 50); // 200ms threshold, 50ms check

    std::mutex mtx;
    std::condition_variable cv;
    std::vector<bool> status_history;

    monitor.start_monitoring([&](bool receiving) {
        {
            std::lock_guard lock(mtx);
            status_history.push_back(receiving);
        }
        cv.notify_all();
    });

    monitor.record_frame();

    // Wait for "receiving = true" callback
    {
        std::unique_lock lock(mtx);
        ASSERT_TRUE(cv.wait_for(lock, 5s,
            [&] { return !status_history.empty() && status_history.back(); }));
    }

    EXPECT_TRUE(monitor.is_receiving_frames());
}

TEST(FrameStaleMonitorTest, BecomesStaleWhenFramesStop) {
    FrameStaleMonitor monitor(0.1, 50); // 100ms threshold, 50ms check

    std::mutex mtx;
    std::condition_variable cv;
    std::vector<bool> status_history;

    monitor.start_monitoring([&](bool receiving) {
        {
            std::lock_guard lock(mtx);
            status_history.push_back(receiving);
        }
        cv.notify_all();
    });

    // Record a frame so it becomes "receiving"
    monitor.record_frame();

    // Wait for "receiving = true" transition
    {
        std::unique_lock lock(mtx);
        ASSERT_TRUE(cv.wait_for(lock, 5s,
            [&] { return !status_history.empty() && status_history.back(); }));
    }

    // Stop recording frames — monitor should detect stale
    // Wait for "receiving = false" transition
    {
        std::unique_lock lock(mtx);
        ASSERT_TRUE(cv.wait_for(lock, 5s,
            [&] { return status_history.size() >= 2 && !status_history.back(); }));
    }

    EXPECT_FALSE(monitor.is_receiving_frames());
}

TEST(FrameStaleMonitorTest, StopMonitoringResetsState) {
    FrameStaleMonitor monitor(0.2, 50);

    std::mutex mtx;
    std::condition_variable cv;
    bool received_first = false;

    monitor.start_monitoring([&](bool receiving) {
        if (receiving) {
            std::lock_guard lock(mtx);
            received_first = true;
            cv.notify_all();
        }
    });

    monitor.record_frame();

    // Wait for first callback
    {
        std::unique_lock lock(mtx);
        cv.wait_for(lock, 5s, [&] { return received_first; });
    }

    monitor.stop_monitoring();

    EXPECT_FALSE(monitor.is_receiving_frames());
}

TEST(FrameStaleMonitorTest, RestartsCleanly) {
    FrameStaleMonitor monitor(0.2, 50);

    // Start and stop twice
    for (int round = 0; round < 2; ++round) {
        std::mutex mtx;
        std::condition_variable cv;
        bool received_status = false;

        monitor.start_monitoring([&](bool /*receiving*/) {
            std::lock_guard lock(mtx);
            received_status = true;
            cv.notify_all();
        });

        monitor.record_frame();

        {
            std::unique_lock lock(mtx);
            ASSERT_TRUE(cv.wait_for(lock, 5s, [&] { return received_status; }))
                << "Round " << round << " failed to receive status callback";
        }

        monitor.stop_monitoring();
    }
}

} // namespace reflection::testing
