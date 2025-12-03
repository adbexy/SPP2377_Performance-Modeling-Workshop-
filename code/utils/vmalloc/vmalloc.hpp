#pragma once

#include <numa.h>

#include "VamPointer.hpp"
#include "VamProphecy.hpp"
#include "vmalloc_defs.hpp"

namespace vampir {

/**
 * @brief Allocate a VamPointer on the default NUMA node 0.
 *
 * @tparam base_t The base type of the elements to allocate.
 * @tparam segment_size_bytes The size of each segment in bytes (default is
 * 4096). A VamPointer is always split at segment boundaries (see
 * VamPointer::split).
 * @param size_elem The number of elements to allocate.
 * @return VamPointer<base_t, segment_size_bytes> The allocated VamPointer.
 */
template <typename base_t, std::size_t segment_size_bytes = 4096>
VamPointer<base_t, segment_size_bytes> vmalloc(std::size_t size_elem) {
  NumaId node = 0;
  DEBUG_VAMPPH(
      "vmalloc called without access pattern; allocating on default NUMA node: "
      << node);
  return VamPointer<base_t, segment_size_bytes>(size_elem, node);
}

/**
 * @brief Allocate a VamPointer on a predicted NUMA node based on the access
 * pattern.
 *
 * @tparam base_t The base type of the elements to allocate.
 * @tparam segment_size_bytes The size of each segment in bytes (default is
 * 4096).
 * @param size_elem The number of elements to allocate.
 * @param pattern The access pattern used for NUMA node prediction.
 * @return VamPointer<base_t, segment_size_bytes> The allocated VamPointer.
 */
template <typename base_t, std::size_t segment_size_bytes = 4096>
VamPointer<base_t, segment_size_bytes> vmalloc(std::size_t size_elem,
                                               AccessPattern pattern) {
  // #### MODIFY: change prediction strategy in VamProphecy if needed
  NumaId node = VamProphecy::predict(pattern);
  // #### end MODIFY
  DEBUG_VAMPPH("vmalloc: access pattern " << access_pattern_to_string(pattern)
                                          << "; predicted NUMA node " << node);
  return VamPointer<base_t, segment_size_bytes>(size_elem, node);
}

} // namespace vampir
