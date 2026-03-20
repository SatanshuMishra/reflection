#include <gtest/gtest.h>

#include "session/DeviceModel.h"

namespace reflection::testing {

TEST(DeviceModelTest, DefaultConstruction) {
    DeviceModel device;
    EXPECT_TRUE(device.id.empty());
    EXPECT_TRUE(device.name.empty());
    EXPECT_TRUE(device.model_id.empty());
    EXPECT_FALSE(device.is_connected);
}

TEST(DeviceModelTest, Equality) {
    DeviceModel a{"id1", "iPad Pro", "iPad13,4", true};
    DeviceModel b{"id1", "iPad Pro", "iPad13,4", true};
    DeviceModel c{"id2", "iPad Air", "iPad13,1", false};

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(DeviceModelTest, CopySemantics) {
    const DeviceModel original{"id1", "iPad Pro", "iPad13,4", true};
    const DeviceModel copy = original;

    EXPECT_EQ(original, copy);
}

} // namespace reflection::testing
