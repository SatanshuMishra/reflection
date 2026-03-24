// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include <gtest/gtest.h>
#include "mdns/MdnsServiceRecord.h"
#include "utilities/Constants.h"

using namespace reflection;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(MdnsServiceRecordTest, DefaultConstruction) {
    const MdnsServiceRecord record;
    EXPECT_TRUE(record.service_name.empty());
    EXPECT_TRUE(record.service_type.empty());
    EXPECT_TRUE(record.hostname.empty());
    EXPECT_EQ(record.port, 0);
    EXPECT_EQ(record.ttl_seconds, 4500);
    EXPECT_TRUE(record.txt_records.empty());
}

TEST(MdnsServiceRecordTest, ManualConstruction) {
    const MdnsServiceRecord record{
        .service_name = "MyService",
        .service_type = "_http._tcp.local.",
        .hostname = "myhost.local.",
        .port = 8080,
        .ttl_seconds = 120,
        .txt_records = {{"key", "value"}}
    };

    EXPECT_EQ(record.service_name, "MyService");
    EXPECT_EQ(record.service_type, "_http._tcp.local.");
    EXPECT_EQ(record.hostname, "myhost.local.");
    EXPECT_EQ(record.port, 8080);
    EXPECT_EQ(record.ttl_seconds, 120);
    ASSERT_EQ(record.txt_records.size(), 1);
    EXPECT_EQ(record.txt_records[0].key, "key");
    EXPECT_EQ(record.txt_records[0].value, "value");
}

// ---------------------------------------------------------------------------
// Equality
// ---------------------------------------------------------------------------

TEST(MdnsServiceRecordTest, EqualRecordsAreEqual) {
    const MdnsServiceRecord a{
        "Svc", "_test._tcp.", "host.", 1234, 60, {{"k", "v"}}
    };
    const MdnsServiceRecord b{
        "Svc", "_test._tcp.", "host.", 1234, 60, {{"k", "v"}}
    };
    EXPECT_EQ(a, b);
}

TEST(MdnsServiceRecordTest, DifferentRecordsAreNotEqual) {
    const MdnsServiceRecord a{"Svc", "_test._tcp.", "host.", 1234, 60, {}};
    const MdnsServiceRecord b{"Other", "_test._tcp.", "host.", 1234, 60, {}};
    EXPECT_NE(a, b);
}

// ---------------------------------------------------------------------------
// TXT Entry equality
// ---------------------------------------------------------------------------

TEST(MdnsTxtEntryTest, EqualEntries) {
    const MdnsTxtEntry a{"features", "0x527FFFF7"};
    const MdnsTxtEntry b{"features", "0x527FFFF7"};
    EXPECT_EQ(a, b);
}

TEST(MdnsTxtEntryTest, DifferentEntries) {
    const MdnsTxtEntry a{"features", "0x527FFFF7"};
    const MdnsTxtEntry b{"features", "0x0"};
    EXPECT_NE(a, b);
}

// ---------------------------------------------------------------------------
// Factory: make_airplay_record
// ---------------------------------------------------------------------------

TEST(MdnsServiceRecordTest, MakeAirplayRecord) {
    const auto record = MdnsServiceRecord::make_airplay_record(
        "Reflection", 7000, "AA:BB:CC:DD:EE:FF");

    EXPECT_EQ(record.service_name, "Reflection");
    EXPECT_EQ(record.service_type, std::string(constants::kAirPlayMdnsType));
    EXPECT_EQ(record.port, 7000);
    EXPECT_EQ(record.ttl_seconds, constants::kMdnsDefaultTtl);

    // Must contain required AirPlay TXT keys
    auto find_txt = [&](const std::string& key) -> std::string {
        for (const auto& entry : record.txt_records) {
            if (entry.key == key) return entry.value;
        }
        return "";
    };

    EXPECT_EQ(find_txt(std::string(constants::kTxtKeyFeatures)),
              std::string(constants::kTxtValueFeatures));
    EXPECT_EQ(find_txt(std::string(constants::kTxtKeyModel)),
              std::string(constants::kTxtValueModel));
    EXPECT_EQ(find_txt(std::string(constants::kTxtKeyFlags)),
              std::string(constants::kTxtValueFlags));
    EXPECT_EQ(find_txt(std::string(constants::kTxtKeySrcvers)),
              std::string(constants::kTxtValueSrcvers));

    // Critical for iOS discovery: pk, pi, deviceid
    EXPECT_EQ(find_txt(std::string(constants::kTxtKeyPk)),
              std::string(constants::kTxtValuePk));
    EXPECT_EQ(find_txt(std::string(constants::kTxtKeyPi)),
              std::string(constants::kTxtValuePi));
    EXPECT_EQ(find_txt(std::string(constants::kTxtKeyDeviceId)),
              "AA:BB:CC:DD:EE:FF");
}

TEST(MdnsServiceRecordTest, MakeAirplayRecordCustomPort) {
    const auto record = MdnsServiceRecord::make_airplay_record(
        "MyPC", 9000, "11:22:33:44:55:66");

    EXPECT_EQ(record.service_name, "MyPC");
    EXPECT_EQ(record.port, 9000);
}

// ---------------------------------------------------------------------------
// Factory: make_raop_record
// ---------------------------------------------------------------------------

TEST(MdnsServiceRecordTest, MakeRaopRecord) {
    const auto record = MdnsServiceRecord::make_raop_record(
        "Reflection", 5000, "AA:BB:CC:DD:EE:FF");

    // RAOP service name format: "<hw_addr_no_colons>@<name>"
    EXPECT_TRUE(record.service_name.find("@Reflection") != std::string::npos);
    EXPECT_EQ(record.service_type, std::string(constants::kRaopMdnsType));
    EXPECT_EQ(record.port, 5000);
}

TEST(MdnsServiceRecordTest, MakeRaopRecordContainsHwAddress) {
    const auto record = MdnsServiceRecord::make_raop_record(
        "Test", 5000, "AA:BB:CC:DD:EE:FF");

    // RAOP names include the hardware address without colons
    EXPECT_TRUE(record.service_name.find("AABBCCDDEEFF") != std::string::npos);
}

TEST(MdnsServiceRecordTest, MakeRaopRecordHasCriticalTxtKeys) {
    const auto record = MdnsServiceRecord::make_raop_record(
        "Test", 5000, "AA:BB:CC:DD:EE:FF");

    auto find_txt = [&](const std::string& key) -> std::string {
        for (const auto& entry : record.txt_records) {
            if (entry.key == key) return entry.value;
        }
        return "";
    };

    // Critical RAOP TXT keys that iOS requires
    EXPECT_EQ(find_txt("txtvers"), "1");
    EXPECT_EQ(find_txt("cn"), "0,1,2,3");   // Audio codecs
    EXPECT_EQ(find_txt("et"), "0,3,5");      // Encryption types
    EXPECT_EQ(find_txt("sr"), "44100");       // Sample rate
    EXPECT_EQ(find_txt("ss"), "16");          // Sample size
    EXPECT_EQ(find_txt("tp"), "UDP");         // Transport
    EXPECT_EQ(find_txt("pw"), "false");       // No password
    EXPECT_EQ(find_txt("pk"), std::string(constants::kTxtValuePk));
    EXPECT_EQ(find_txt("am"), std::string(constants::kTxtValueModel));
    EXPECT_EQ(find_txt("vs"), std::string(constants::kTxtValueSrcvers));
}

// ---------------------------------------------------------------------------
// Immutability (copies are independent)
// ---------------------------------------------------------------------------

TEST(MdnsServiceRecordTest, CopiesAreIndependent) {
    MdnsServiceRecord original{
        "Svc", "_test._tcp.", "host.", 1234, 60, {{"k", "v"}}
    };
    MdnsServiceRecord copy = original;

    EXPECT_EQ(original, copy);

    // Modifying copy does not affect original
    copy.service_name = "Modified";
    EXPECT_NE(original.service_name, copy.service_name);
    EXPECT_EQ(original.service_name, "Svc");
}
