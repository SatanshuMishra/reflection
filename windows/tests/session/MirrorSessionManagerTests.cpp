#include <gtest/gtest.h>

#include "session/MirrorSessionManager.h"
#include "capture/MockScreenCapture.h"

#include <memory>
#include <vector>

namespace reflection::testing {

class MirrorSessionManagerTest : public ::testing::Test {
protected:
    // Track created mocks for test assertions
    std::vector<MockScreenCapture*> created_mocks;

    CaptureFactory make_factory() {
        return [this](const std::string& /*device_id*/)
            -> std::unique_ptr<IScreenCapture> {
            auto mock = std::make_unique<MockScreenCapture>();
            created_mocks.push_back(mock.get());
            return mock;
        };
    }

    CaptureFactory make_failing_factory(CaptureErrorCode code) {
        return [this, code](const std::string& /*device_id*/)
            -> std::unique_ptr<IScreenCapture> {
            auto mock = std::make_unique<MockScreenCapture>();
            mock->should_throw_on_start = CaptureError(code);
            created_mocks.push_back(mock.get());
            return mock;
        };
    }
};

TEST_F(MirrorSessionManagerTest, StartMirroringCreatesSession) {
    MirrorSessionManager manager(make_factory());

    manager.start_mirroring("device1");

    EXPECT_TRUE(manager.has_session("device1"));
    EXPECT_EQ(created_mocks.size(), 1u);
    EXPECT_EQ(created_mocks[0]->start_capture_call_count, 1);
}

TEST_F(MirrorSessionManagerTest, DuplicateStartIsNoOp) {
    MirrorSessionManager manager(make_factory());

    manager.start_mirroring("device1");
    manager.start_mirroring("device1"); // Should be no-op

    EXPECT_EQ(created_mocks.size(), 1u);
    EXPECT_EQ(created_mocks[0]->start_capture_call_count, 1);
}

TEST_F(MirrorSessionManagerTest, StopMirroringRemovesSession) {
    MirrorSessionManager manager(make_factory());

    manager.start_mirroring("device1");
    EXPECT_TRUE(manager.has_session("device1"));

    manager.stop_mirroring("device1");
    EXPECT_FALSE(manager.has_session("device1"));
    EXPECT_EQ(created_mocks[0]->stop_capture_call_count, 1);
}

TEST_F(MirrorSessionManagerTest, StopNonExistentIsNoOp) {
    MirrorSessionManager manager(make_factory());
    manager.stop_mirroring("nonexistent"); // Should not crash
}

TEST_F(MirrorSessionManagerTest, ErrorOnStartSurfacesError) {
    MirrorSessionManager manager(make_failing_factory(
        CaptureErrorCode::NetworkError
    ));

    bool error_fired = false;
    manager.set_error_callback([&](const CaptureError& e) {
        error_fired = true;
        EXPECT_EQ(e.code(), CaptureErrorCode::NetworkError);
    });

    manager.start_mirroring("device1");

    EXPECT_TRUE(error_fired);
    EXPECT_FALSE(manager.has_session("device1"));
    EXPECT_TRUE(manager.current_error().has_value());
}

TEST_F(MirrorSessionManagerTest, ClearErrorResetsState) {
    MirrorSessionManager manager(make_failing_factory(
        CaptureErrorCode::NetworkError
    ));

    manager.start_mirroring("device1");
    EXPECT_TRUE(manager.current_error().has_value());

    manager.clear_error();
    EXPECT_FALSE(manager.current_error().has_value());
}

TEST_F(MirrorSessionManagerTest, MultipleDevicesAreIndependent) {
    MirrorSessionManager manager(make_factory());

    manager.start_mirroring("device1");
    manager.start_mirroring("device2");

    EXPECT_TRUE(manager.has_session("device1"));
    EXPECT_TRUE(manager.has_session("device2"));
    EXPECT_EQ(created_mocks.size(), 2u);

    manager.stop_mirroring("device1");
    EXPECT_FALSE(manager.has_session("device1"));
    EXPECT_TRUE(manager.has_session("device2"));
}

TEST_F(MirrorSessionManagerTest, WindowCloseStopsSession) {
    MirrorSessionManager manager(make_factory());

    manager.start_mirroring("device1");
    EXPECT_TRUE(manager.has_session("device1"));

    manager.on_mirror_window_closed("device1");
    EXPECT_FALSE(manager.has_session("device1"));
}

TEST_F(MirrorSessionManagerTest, SessionChangeCallbackFired) {
    MirrorSessionManager manager(make_factory());

    std::vector<std::pair<std::string, bool>> changes;
    manager.set_session_change_callback([&](const std::string& id, bool active) {
        changes.emplace_back(id, active);
    });

    manager.start_mirroring("device1");
    manager.stop_mirroring("device1");

    ASSERT_EQ(changes.size(), 2u);
    EXPECT_EQ(changes[0], std::make_pair(std::string("device1"), true));
    EXPECT_EQ(changes[1], std::make_pair(std::string("device1"), false));
}

} // namespace reflection::testing
