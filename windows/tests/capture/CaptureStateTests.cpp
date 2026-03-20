#include <gtest/gtest.h>

#include "capture/CaptureState.h"
#include "capture/CaptureError.h"

namespace reflection::testing {

TEST(CaptureStateTest, LabelReturnsCorrectStrings) {
    EXPECT_STREQ(capture_state_label(CaptureState::Idle), "Idle");
    EXPECT_STREQ(capture_state_label(CaptureState::Starting), "Starting");
    EXPECT_STREQ(capture_state_label(CaptureState::Running), "Running");
    EXPECT_STREQ(capture_state_label(CaptureState::Stopped), "Stopped");
    EXPECT_STREQ(capture_state_label(CaptureState::Failed), "Failed");
}

TEST(CaptureErrorTest, DeviceNotFoundHasUserMessage) {
    const CaptureError error(CaptureErrorCode::DeviceNotFound);
    EXPECT_FALSE(error.user_message().empty());
    EXPECT_EQ(error.code(), CaptureErrorCode::DeviceNotFound);
}

TEST(CaptureErrorTest, SessionConfigFailedIncludesDetail) {
    const CaptureError error(
        CaptureErrorCode::SessionConfigurationFailed, "bad codec"
    );
    EXPECT_NE(error.user_message().find("bad codec"), std::string::npos);
}

TEST(CaptureErrorTest, DeviceDisconnectedMessage) {
    const CaptureError error(CaptureErrorCode::DeviceDisconnected);
    EXPECT_NE(error.user_message().find("disconnected"), std::string::npos);
}

TEST(CaptureErrorTest, FirewallBlockedMessage) {
    const CaptureError error(CaptureErrorCode::FirewallBlocked);
    EXPECT_NE(error.user_message().find("Firewall"), std::string::npos);
}

TEST(CaptureErrorTest, UnknownErrorWithDetail) {
    const CaptureError error(CaptureErrorCode::UnknownError, "something broke");
    EXPECT_NE(error.user_message().find("something broke"), std::string::npos);
}

TEST(CaptureErrorTest, UnknownErrorWithoutDetail) {
    const CaptureError error(CaptureErrorCode::UnknownError);
    EXPECT_FALSE(error.user_message().empty());
}

} // namespace reflection::testing
