// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include <gtest/gtest.h>

#include "utilities/UpdateChecker.h"

namespace reflection::testing {

// Additional version comparison edge cases

TEST(VersionComparisonTest, TwoComponentVersions) {
    EXPECT_TRUE(UpdateChecker::is_newer("2.0", "1.5"));
    EXPECT_FALSE(UpdateChecker::is_newer("1.5", "2.0"));
}

TEST(VersionComparisonTest, SingleComponentVersions) {
    EXPECT_TRUE(UpdateChecker::is_newer("2", "1"));
    EXPECT_FALSE(UpdateChecker::is_newer("1", "2"));
}

TEST(VersionComparisonTest, MixedComponentLengths) {
    // "1.0.1" vs "1.0" — 1.0.1 is newer
    EXPECT_TRUE(UpdateChecker::is_newer("1.0.1", "1.0"));
    // "1.0" vs "1.0.1" — 1.0 is older
    EXPECT_FALSE(UpdateChecker::is_newer("1.0", "1.0.1"));
}

TEST(VersionComparisonTest, LargeVersionNumbers) {
    EXPECT_TRUE(UpdateChecker::is_newer("10.20.300", "10.20.299"));
    EXPECT_FALSE(UpdateChecker::is_newer("10.20.299", "10.20.300"));
}

} // namespace reflection::testing
