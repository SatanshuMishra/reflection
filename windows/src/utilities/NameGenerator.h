#pragma once

#include <string>

namespace reflection {

/// Generates random adjective-noun names for AirPlay server identity.
/// Pattern: "adjective-noun" (e.g., "stellar-penguin", "cosmic-aurora")
/// Inspired by Astro.js, Vercel, Railway random project names.
class NameGenerator {
public:
    /// Generate a single random name.
    [[nodiscard]] static std::string generate();
};

} // namespace reflection
