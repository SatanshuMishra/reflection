// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include <gtest/gtest.h>

#include "utilities/UpdateChecker.h"

// Note: Full AppSettings tests require Windows Registry access.
// These tests focus on the UpdateChecker version comparison logic
// which is pure and testable on any platform.

namespace reflection::testing {

TEST(UpdateCheckerTest, NewerMajorVersion) {
    EXPECT_TRUE(UpdateChecker::is_newer("2.0.0", "1.5.8"));
}

TEST(UpdateCheckerTest, NewerMinorVersion) {
    EXPECT_TRUE(UpdateChecker::is_newer("1.6.0", "1.5.8"));
}

TEST(UpdateCheckerTest, NewerPatchVersion) {
    EXPECT_TRUE(UpdateChecker::is_newer("1.5.9", "1.5.8"));
}

TEST(UpdateCheckerTest, SameVersionIsNotNewer) {
    EXPECT_FALSE(UpdateChecker::is_newer("1.5.8", "1.5.8"));
}

TEST(UpdateCheckerTest, OlderVersionIsNotNewer) {
    EXPECT_FALSE(UpdateChecker::is_newer("1.5.7", "1.5.8"));
    EXPECT_FALSE(UpdateChecker::is_newer("1.4.0", "1.5.8"));
    EXPECT_FALSE(UpdateChecker::is_newer("0.9.0", "1.5.8"));
}

TEST(UpdateCheckerTest, HandlesVPrefix) {
    EXPECT_TRUE(UpdateChecker::is_newer("v2.0.0", "v1.5.8"));
    EXPECT_TRUE(UpdateChecker::is_newer("v1.6.0", "1.5.8"));
}

TEST(UpdateCheckerTest, HandlesVwPrefix) {
    EXPECT_TRUE(UpdateChecker::is_newer("vw2.0.0", "vw1.0.0"));
    EXPECT_TRUE(UpdateChecker::is_newer("vw1.1.0", "vw1.0.0"));
}

TEST(UpdateCheckerTest, HandlesDifferentLengths) {
    EXPECT_TRUE(UpdateChecker::is_newer("1.5.8.1", "1.5.8"));
    EXPECT_FALSE(UpdateChecker::is_newer("1.5.8", "1.5.8.1"));
}

TEST(UpdateCheckerTest, HandlesEmptyStrings) {
    EXPECT_FALSE(UpdateChecker::is_newer("", "1.0.0"));
    EXPECT_FALSE(UpdateChecker::is_newer("", ""));
}

} // namespace reflection::testing
