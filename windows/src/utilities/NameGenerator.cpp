#include "utilities/NameGenerator.h"

#include <array>
#include <random>

namespace reflection {

namespace {

constexpr std::array kAdjectives = {
    "stellar",  "cosmic",   "bright",   "swift",    "gentle",
    "frosted",  "golden",   "silent",   "verdant",  "amber",
    "crystal",  "lunar",    "vivid",    "radiant",  "serene",
    "coral",    "misty",    "polished", "nimble",   "velvet",
    "prismic",  "azure",    "crimson",  "ivory",    "jade",
    "onyx",     "scarlet",  "topaz",    "cobalt",   "indigo",
    "rustic",   "sleek",    "bold",     "calm",     "clever",
    "daring",   "eager",    "fierce",   "graceful", "humble",
    "keen",     "lively",   "merry",    "noble",    "patient",
    "quiet",    "rapid",    "subtle",   "tender",   "witty",
    "ancient",  "blazing",  "charmed",  "drifting", "electric",
    "floating", "glowing",  "hidden",   "infinite", "jolly",
};

constexpr std::array kNouns = {
    "penguin",  "falcon",   "aurora",   "nebula",   "crystal",
    "meadow",   "harbor",   "summit",   "prism",    "atlas",
    "canyon",   "delta",    "ember",    "frost",    "grove",
    "haven",    "iris",     "jasper",   "kite",     "lotus",
    "maple",    "nova",     "opal",     "pearl",    "quartz",
    "reef",     "sage",     "timber",   "umber",    "vale",
    "willow",   "zenith",   "brook",    "cedar",    "dune",
    "flint",    "gale",     "heron",    "isle",     "jade",
    "lark",     "moon",     "north",    "orchid",   "pine",
    "ridge",    "shore",    "tide",     "vine",     "wren",
    "anchor",   "beacon",   "cliff",    "dawn",     "echo",
    "flame",    "glacier",  "horizon",  "inlet",    "jewel",
};

} // anonymous namespace

std::string NameGenerator::generate() {
    // Thread-local RNG for safety
    thread_local std::mt19937 rng{std::random_device{}()};

    std::uniform_int_distribution<size_t> adj_dist(0, kAdjectives.size() - 1);
    std::uniform_int_distribution<size_t> noun_dist(0, kNouns.size() - 1);

    return std::string(kAdjectives[adj_dist(rng)]) + "-" +
           std::string(kNouns[noun_dist(rng)]);
}

} // namespace reflection
