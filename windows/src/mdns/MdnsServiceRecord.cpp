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

/// Build the _airplay._tcp TXT records matching RPiPlay's dnssd.c.
/// These records are what iOS uses to identify AirPlay capabilities.
std::vector<MdnsTxtEntry> make_airplay_txt(const std::string& hw_addr_hex) {
    return {
        {std::string(constants::kTxtKeyDeviceId), hw_addr_hex},
        {std::string(constants::kTxtKeyFeatures), std::string(constants::kTxtValueFeatures)},
        {std::string(constants::kTxtKeyFlags),    std::string(constants::kTxtValueFlags)},
        {std::string(constants::kTxtKeyModel),    std::string(constants::kTxtValueModel)},
        {std::string(constants::kTxtKeyPk),       std::string(constants::kTxtValuePk)},
        {std::string(constants::kTxtKeyPi),       std::string(constants::kTxtValuePi)},
        {std::string(constants::kTxtKeySrcvers),  std::string(constants::kTxtValueSrcvers)},
        {std::string(constants::kTxtKeyVv),       std::string(constants::kTxtValueVv)},
    };
}

/// Build the _raop._tcp TXT records matching RPiPlay's dnssd.c.
std::vector<MdnsTxtEntry> make_raop_txt() {
    return {
        {"txtvers", "1"},
        {"ch", "2"},                // Audio channels
        {"cn", "0,1,2,3"},          // Audio codecs: PCM, ALAC, AAC, AAC-ELD
        {"da", "true"},
        {"et", "0,3,5"},            // Encryption types
        {"ft", std::string(constants::kTxtValueFeatures)},
        {"md", "0,1,2"},            // Metadata types
        {"rhd", "5.6.0.0"},
        {"pw", "false"},            // No password
        {"sr", "44100"},            // Sample rate
        {"ss", "16"},               // Sample size
        {"sv", "false"},
        {"tp", "UDP"},              // Transport protocol
        {"vs", std::string(constants::kTxtValueSrcvers)},
        {"vn", "65537"},
        {"sf", std::string(constants::kTxtValueFlags)},
        {"pk", std::string(constants::kTxtValuePk)},
        {"am", std::string(constants::kTxtValueModel)},
    };
}

} // anonymous namespace

MdnsServiceRecord MdnsServiceRecord::make_airplay_record(
    const std::string& server_name,
    uint16_t port,
    const std::string& hw_addr_hex
) {
    return MdnsServiceRecord{
        .service_name = server_name,
        .service_type = std::string(constants::kAirPlayMdnsType),
        .hostname = server_name + ".local.",
        .port = port,
        .ttl_seconds = constants::kMdnsDefaultTtl,
        .txt_records = make_airplay_txt(hw_addr_hex),
    };
}

MdnsServiceRecord MdnsServiceRecord::make_raop_record(
    const std::string& server_name,
    uint16_t port,
    const std::string& hw_addr_hex
) {
    // RAOP service name format: "<hw_addr_no_colons>@<server_name>"
    const std::string raop_name = strip_colons(hw_addr_hex) + "@" + server_name;

    return MdnsServiceRecord{
        .service_name = raop_name,
        .service_type = std::string(constants::kRaopMdnsType),
        .hostname = server_name + ".local.",
        .port = port,
        .ttl_seconds = constants::kMdnsDefaultTtl,
        .txt_records = make_raop_txt(),
    };
}

} // namespace reflection
