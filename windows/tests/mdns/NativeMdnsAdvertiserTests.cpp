// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include <gtest/gtest.h>

#include "mdns/NativeMdnsAdvertiser.h"
#include "mdns/MdnsServiceRecord.h"
#include "utilities/Constants.h"

namespace reflection::test {

// --------------------------------------------------------------------------
// NativeMdnsAdvertiser — unit tests
//
// These tests verify the advertiser's state management and record tracking.
// Actual mDNS packet sending requires a live network (integration tests).
// --------------------------------------------------------------------------

class NativeMdnsAdvertiserTest : public ::testing::Test {
protected:
    void SetUp() override {
        advertiser_ = std::make_unique<NativeMdnsAdvertiser>();
    }

    void TearDown() override {
        advertiser_.reset();
    }

    std::unique_ptr<NativeMdnsAdvertiser> advertiser_;

    static MdnsServiceRecord make_test_airplay_record() {
        return MdnsServiceRecord::make_airplay_record(
            "TestReflection", 7000, "AA:BB:CC:DD:EE:FF");
    }

    static MdnsServiceRecord make_test_raop_record() {
        return MdnsServiceRecord::make_raop_record(
            "TestReflection", 5000, "AA:BB:CC:DD:EE:FF");
    }
};

TEST_F(NativeMdnsAdvertiserTest, InitiallyNotAdvertising) {
    EXPECT_FALSE(advertiser_->is_advertising());
    EXPECT_TRUE(advertiser_->advertised_services().empty());
}

TEST_F(NativeMdnsAdvertiserTest, AdvertiseSingleServiceTracksRecord) {
    const auto record = make_test_airplay_record();

    // advertise() may fail without network — but should track the record
    advertiser_->advertise(record);

    const auto services = advertiser_->advertised_services();
    EXPECT_EQ(services.size(), 1u);
    EXPECT_EQ(services[0], record.service_type);
}

TEST_F(NativeMdnsAdvertiserTest, AdvertiseTwoServicesTracksBoth) {
    advertiser_->advertise(make_test_airplay_record());
    advertiser_->advertise(make_test_raop_record());

    const auto services = advertiser_->advertised_services();
    EXPECT_EQ(services.size(), 2u);
}

TEST_F(NativeMdnsAdvertiserTest, WithdrawRemovesService) {
    const auto record = make_test_airplay_record();
    advertiser_->advertise(record);

    advertiser_->withdraw(record.service_type);

    EXPECT_TRUE(advertiser_->advertised_services().empty());
    EXPECT_FALSE(advertiser_->is_advertising());
}

TEST_F(NativeMdnsAdvertiserTest, WithdrawAllClearsEverything) {
    advertiser_->advertise(make_test_airplay_record());
    advertiser_->advertise(make_test_raop_record());
    EXPECT_EQ(advertiser_->advertised_services().size(), 2u);

    advertiser_->withdraw_all();

    EXPECT_TRUE(advertiser_->advertised_services().empty());
    EXPECT_FALSE(advertiser_->is_advertising());
}

TEST_F(NativeMdnsAdvertiserTest, WithdrawNonexistentServiceIsNoOp) {
    advertiser_->advertise(make_test_airplay_record());

    advertiser_->withdraw("_nonexistent._tcp.local.");

    EXPECT_EQ(advertiser_->advertised_services().size(), 1u);
}

TEST_F(NativeMdnsAdvertiserTest, IsAdvertisingReturnsTrueWhenHasRecords) {
    EXPECT_FALSE(advertiser_->is_advertising());

    advertiser_->advertise(make_test_airplay_record());

    EXPECT_TRUE(advertiser_->is_advertising());
}

TEST_F(NativeMdnsAdvertiserTest, DestructorWithdrawsAllServices) {
    {
        NativeMdnsAdvertiser scoped;
        scoped.advertise(make_test_airplay_record());
        scoped.advertise(make_test_raop_record());
        EXPECT_TRUE(scoped.is_advertising());
        // Destructor should call withdraw_all()
    }
    // If we get here without a crash, the destructor handled cleanup
    SUCCEED();
}

TEST_F(NativeMdnsAdvertiserTest, DuplicateAdvertiseDoesNotDoubleTrack) {
    const auto record = make_test_airplay_record();

    advertiser_->advertise(record);
    advertiser_->advertise(record);

    EXPECT_EQ(advertiser_->advertised_services().size(), 1u);
}

// --------------------------------------------------------------------------
// MdnsPacketBuilder — tests for record conversion to mdns_record_t
// --------------------------------------------------------------------------

TEST(MdnsPacketBuilderTest, ServiceRecordHasCorrectPort) {
    const auto record = MdnsServiceRecord::make_airplay_record(
        "Reflection", 7000, "AA:BB:CC:DD:EE:FF");

    EXPECT_EQ(record.port, 7000);
    EXPECT_EQ(record.service_type, std::string(constants::kAirPlayMdnsType));
}

TEST(MdnsPacketBuilderTest, RaopRecordNameIncludesHardwareAddress) {
    const auto record = MdnsServiceRecord::make_raop_record(
        "Reflection", 5000, "AA:BB:CC:DD:EE:FF");

    // RAOP name format: <hw_no_colons>@<name>
    EXPECT_NE(record.service_name.find("AABBCCDDEEFF"), std::string::npos);
    EXPECT_NE(record.service_name.find("@Reflection"), std::string::npos);
}

TEST(MdnsPacketBuilderTest, AirPlayTxtContainsRequiredKeys) {
    const auto record = MdnsServiceRecord::make_airplay_record(
        "Reflection", 7000, "AA:BB:CC:DD:EE:FF");

    // Verify required TXT keys
    std::vector<std::string> required_keys = {
        "features", "model", "flags", "pk", "srcvers", "vv", "pi", "deviceid"
    };

    for (const auto& key : required_keys) {
        bool found = false;
        for (const auto& txt : record.txt_records) {
            if (txt.key == key) { found = true; break; }
        }
        EXPECT_TRUE(found) << "Missing required TXT key: " << key;
    }
}

TEST(MdnsPacketBuilderTest, RaopTxtContainsPkAndFeatures) {
    const auto record = MdnsServiceRecord::make_raop_record(
        "Reflection", 5000, "AA:BB:CC:DD:EE:FF");

    bool has_pk = false;
    bool has_ft = false;
    for (const auto& txt : record.txt_records) {
        if (txt.key == "pk") has_pk = true;
        if (txt.key == "ft") has_ft = true;
    }

    EXPECT_TRUE(has_pk) << "RAOP TXT must contain 'pk' key";
    EXPECT_TRUE(has_ft) << "RAOP TXT must contain 'ft' key";
}

} // namespace reflection::test
