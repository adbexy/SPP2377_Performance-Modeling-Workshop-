#pragma once

#include "MemoryConfig.hpp"

#include "vmalloc_defs.hpp"

namespace vampir {

class VamProphecy {

public:
  // #### MODIFY: change prediction algorithm and interface
  static NumaId predict(AccessPattern pattern) {
    if (pattern == AccessPattern::LINEAR) {
      try {
        return mem_config.get_first_node(Memory::HBM);
      } catch (const std::invalid_argument &e) {
        // Fallback to any if no HBM node is available
        return mem_config.get_first_node();
      }
    } else {
      return mem_config.get_first_node(Memory::DRAM);
    }
  }
  // #### end MODIFY
};

} // namespace vampir
