// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include "mdns/IMdnsAdvertiser.h"

#include <algorithm>

namespace reflection::testing {

/// Mock mDNS advertiser for testing AirPlayService orchestration.
class MockMdnsAdvertiser : public IMdnsAdvertiser {
public:
    // Configurable behavior
    bool should_fail_advertise = false;

    // Call tracking
    int advertise_call_count = 0;
    int withdraw_call_count = 0;
    int withdraw_all_call_count = 0;

    // Captured state
    std::vector<MdnsServiceRecord> advertised_records;

    bool advertise(const MdnsServiceRecord& record) override {
        ++advertise_call_count;
        if (should_fail_advertise) return false;
        advertised_records.push_back(record);
        return true;
    }

    void withdraw(const std::string& service_type) override {
        ++withdraw_call_count;
        std::erase_if(advertised_records,
            [&](const MdnsServiceRecord& r) {
                return r.service_type == service_type;
            });
    }

    void withdraw_all() override {
        ++withdraw_all_call_count;
        advertised_records.clear();
    }

    [[nodiscard]] bool is_advertising() const override {
        return !advertised_records.empty();
    }

    [[nodiscard]] std::vector<std::string> advertised_services() const override {
        std::vector<std::string> result;
        for (const auto& r : advertised_records) {
            result.push_back(r.service_type);
        }
        return result;
    }
};

} // namespace reflection::testing
