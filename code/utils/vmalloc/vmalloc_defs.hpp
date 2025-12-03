#pragma once

#include <cstdint>

#include "vmalloc_debug.hpp"

namespace vampir {

using NumaId = int;

enum class AccessPattern { LINEAR, RANDOM };

static AccessPattern access_pattern_from_string(const std::string &str) {
	if (str == "LINEAR") return AccessPattern::LINEAR;
	else if (str == "RANDOM") return AccessPattern::RANDOM;
	else { throw std::invalid_argument("Unknown access pattern: " + str); }
}

static std::string access_pattern_to_string(AccessPattern pattern) {
	if (pattern == AccessPattern::LINEAR) return "LINEAR";
	else if (pattern == AccessPattern::RANDOM) return "RANDOM";
	else {
		throw std::invalid_argument(
		    "No string representation for access pattern: " + std::to_string(static_cast<int>(pattern)));
	}
}

enum class Memory { DRAM, HBM };

static Memory memory_from_string(const std::string &str) {
	if (str == "DRAM") return Memory::DRAM;
	else if (str == "HBM") return Memory::HBM;
	else { throw std::invalid_argument("Unknown memory type: " + str); }
}

static std::string memory_to_string(Memory mem) {
	if (mem == Memory::DRAM) return "DRAM";
	else if (mem == Memory::HBM) return "HBM";
	else {
		throw std::invalid_argument(
		    "No string representation for memory type: " + std::to_string(static_cast<int>(mem)));
	}
}

} // namespace vampir
