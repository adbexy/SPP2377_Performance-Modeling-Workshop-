#pragma once

#include <fstream>

#include "vmalloc_defs.hpp"

#include "../../modules/json/single_include/nlohmann/json.hpp"

namespace vampir {

class MemoryConfig {

private:
  // mapping from numa node to memory type
  std::unordered_map<NumaId, Memory> node_memory_map;

public:
  static MemoryConfig load_config(const std::string &path) {
    std::ifstream file(path);
    if (!file.is_open()) {
      throw std::ios_base::failure("Could not open file: " + path);
    }

    std::string json_str((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    return MemoryConfig(json_str);
  }

  MemoryConfig(const std::string &json_str) {
    nlohmann::json json_obj = nlohmann::json::parse(json_str);
    for (auto node : json_obj["nodes"]) {
      NumaId node_id = node["node"];
      Memory mem_type = memory_from_string(node["mem_type"]);

      node_memory_map[node_id] = mem_type;
    }
  }

  NumaId get_first_node(Memory mem_type) const {
    NumaId min_node = std::numeric_limits<NumaId>::max();
    for (const auto &[node_id, type] : node_memory_map) {
      if (type == mem_type) {
        min_node = std::min(min_node, node_id);
      }
    }

    if (min_node != std::numeric_limits<NumaId>::max())
      return min_node;
    else
      throw std::invalid_argument("No NUMA node found for memory type");
  }

  NumaId get_first_node() const {
    if (node_memory_map.empty()) {
      throw std::invalid_argument(
          "No NUMA nodes available in memory configuration");
    }

    NumaId min_node = std::numeric_limits<NumaId>::max();
    for (const auto &[node_id, type] : node_memory_map) {
      min_node = std::min(min_node, node_id);
    }
    return min_node;
  }
};

#if TESTING
  MemoryConfig mem_config =
  MemoryConfig::load_config("code/utils/vmalloc/crobat_testing_config.json");
#else
  MemoryConfig mem_config =
  MemoryConfig::load_config("code/utils/vmalloc/crobat_benchmarking_config.json");
#endif
// MemoryConfig mem_config =
// MemoryConfig::load_config("code/utils/vmalloc/laptop_config.json");
} // namespace vampir
