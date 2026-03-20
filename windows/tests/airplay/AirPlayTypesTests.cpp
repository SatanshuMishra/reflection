#include <gtest/gtest.h>
#include "airplay/AirPlayTypes.h"

using namespace reflection;

// ---------------------------------------------------------------------------
// AirPlayServiceConfig
// ---------------------------------------------------------------------------

TEST(AirPlayServiceConfigTest, DefaultValues) {
    const AirPlayServiceConfig config;
    EXPECT_EQ(config.server_name, "Reflection");
    EXPECT_EQ(config.raop_port, 5000);
    EXPECT_EQ(config.airplay_port, 7000);
    EXPECT_EQ(config.hardware_address.size(), 6);
}

TEST(AirPlayServiceConfigTest, Equality) {
    const AirPlayServiceConfig a;
    const AirPlayServiceConfig b;
    EXPECT_EQ(a, b);
}

TEST(AirPlayServiceConfigTest, Inequality) {
    AirPlayServiceConfig a;
    AirPlayServiceConfig b;
    b.server_name = "Other";
    EXPECT_NE(a, b);
}

// ---------------------------------------------------------------------------
// hw_address_hex
// ---------------------------------------------------------------------------

TEST(AirPlayServiceConfigTest, HwAddressHexDefault) {
    const AirPlayServiceConfig config;
    EXPECT_EQ(config.hw_address_hex(), "AA:BB:CC:DD:EE:FF");
}

TEST(AirPlayServiceConfigTest, HwAddressHexCustom) {
    AirPlayServiceConfig config;
    config.hardware_address = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    EXPECT_EQ(config.hw_address_hex(), "11:22:33:44:55:66");
}

TEST(AirPlayServiceConfigTest, HwAddressHexWithZeros) {
    AirPlayServiceConfig config;
    config.hardware_address = {0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E};
    EXPECT_EQ(config.hw_address_hex(), "00:0A:0B:0C:0D:0E");
}

// ---------------------------------------------------------------------------
// AirPlayConnectionState
// ---------------------------------------------------------------------------

TEST(AirPlayConnectionStateTest, AllStatesExist) {
    EXPECT_NE(AirPlayConnectionState::Disconnected,
              AirPlayConnectionState::Connecting);
    EXPECT_NE(AirPlayConnectionState::Connected,
              AirPlayConnectionState::Disconnecting);
}

// ---------------------------------------------------------------------------
// AirPlayClientInfo
// ---------------------------------------------------------------------------

TEST(AirPlayClientInfoTest, DefaultValues) {
    const AirPlayClientInfo info;
    EXPECT_TRUE(info.device_id.empty());
    EXPECT_TRUE(info.device_name.empty());
    EXPECT_TRUE(info.device_model.empty());
    EXPECT_EQ(info.width, 0);
    EXPECT_EQ(info.height, 0);
}
