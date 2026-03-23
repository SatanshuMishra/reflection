#include "mdns/MdnsServiceRecord.h"
#include "utilities/Constants.h"

#include <algorithm>

namespace reflection {

namespace {

/// Remove colons from a hardware address string.
/// "AA:BB:CC:DD:EE:FF" -> "AABBCCDDEEFF"
std::string strip_colons(const std::string& hw_addr) {
    std::string result;
    result.reserve(12);
    for (char c : hw_addr) {
        if (c != ':') result += c;
    }
    return result;
}

/// Build the _airplay._tcp TXT records.
/// If pk_override is non-empty, use it instead of the hardcoded default.
std::vector<MdnsTxtEntry> make_airplay_txt(const std::string& hw_addr_hex,
                                            const std::string& pk_override) {
    const std::string pk = pk_override.empty()
        ? std::string(constants::kTxtValuePk) : pk_override;
    return {
        {std::string(constants::kTxtKeyDeviceId), hw_addr_hex},
        {std::string(constants::kTxtKeyFeatures), std::string(constants::kTxtValueFeatures)},
        {std::string(constants::kTxtKeyFlags),    std::string(constants::kTxtValueFlags)},
        {std::string(constants::kTxtKeyModel),    std::string(constants::kTxtValueModel)},
        {std::string(constants::kTxtKeyPk),       pk},
        {std::string(constants::kTxtKeyPi),       std::string(constants::kTxtValuePi)},
        {std::string(constants::kTxtKeySrcvers),  std::string(constants::kTxtValueSrcvers)},
        {std::string(constants::kTxtKeyVv),       std::string(constants::kTxtValueVv)},
    };
}

/// Build the _raop._tcp TXT records.
std::vector<MdnsTxtEntry> make_raop_txt(const std::string& pk_override) {
    const std::string pk = pk_override.empty()
        ? std::string(constants::kTxtValuePk) : pk_override;
    return {
        {"txtvers", "1"},
        {"ch", "2"},
        {"cn", "0,1,2,3"},
        {"da", "true"},
        {"et", "0,3,5"},
        {"ft", std::string(constants::kTxtValueFeatures)},
        {"md", "0,1,2"},
        {"rhd", "5.6.0.0"},
        {"pw", "false"},
        {"sr", "44100"},
        {"ss", "16"},
        {"sv", "false"},
        {"tp", "UDP"},
        {"vs", std::string(constants::kTxtValueSrcvers)},
        {"vn", "65537"},
        {"sf", std::string(constants::kTxtValueFlags)},
        {"pk", pk},
        {"am", std::string(constants::kTxtValueModel)},
    };
}

} // anonymous namespace

MdnsServiceRecord MdnsServiceRecord::make_airplay_record(
    const std::string& server_name,
    uint16_t port,
    const std::string& hw_addr_hex,
    const std::string& pk_override
) {
    return MdnsServiceRecord{
        .service_name = server_name,
        .service_type = std::string(constants::kAirPlayMdnsType),
        .hostname = server_name + ".local.",
        .port = port,
        .ttl_seconds = constants::kMdnsDefaultTtl,
        .txt_records = make_airplay_txt(hw_addr_hex, pk_override),
    };
}

MdnsServiceRecord MdnsServiceRecord::make_raop_record(
    const std::string& server_name,
    uint16_t port,
    const std::string& hw_addr_hex,
    const std::string& pk_override
) {
    const std::string raop_name = strip_colons(hw_addr_hex) + "@" + server_name;

    return MdnsServiceRecord{
        .service_name = raop_name,
        .service_type = std::string(constants::kRaopMdnsType),
        .hostname = server_name + ".local.",
        .port = port,
        .ttl_seconds = constants::kMdnsDefaultTtl,
        .txt_records = make_raop_txt(pk_override),
    };
}

} // namespace reflection
