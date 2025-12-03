#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <numa.h>
#include <tuple>
#include <vector>

#include "vmalloc_defs.hpp"

namespace vampir {

struct AllocationInfo {
  NumaId numa_node;
  void *data;
  std::size_t size_bytes; // in bytes
  std::atomic<uint32_t> ref_cnt;
};

template <typename base_t, std::size_t segment_size_bytes = 4096>
class VamPointer {
  // grants access to private members of all VamPointer specializations (for
  // _copy_attr, _free_ptr, _unset_attr)
  template <typename, std::size_t> friend class VamPointer;

private:
  using this_t = VamPointer<base_t, segment_size_bytes>;

  /**
   * @brief Pointer to allocation info structure. Is created once per vmalloc
   * call and used for reference countring (VamPointer frees memory, when last
   * referencing pointer is destructed).
   */
  AllocationInfo *alloc_info;
  base_t *start;
  std::size_t size_bytes;

private:
  template <typename T, std::size_t SSIZE_0, typename U, std::size_t SSIZE_1>
  static void _copy_attr(const VamPointer<T, SSIZE_0> &from,
                         VamPointer<U, SSIZE_1> &to) {
    to.alloc_info = from.alloc_info;
    to.size_bytes = from.size_bytes;
    to.start = reinterpret_cast<U *>(from.start);
  }

  template <typename T, std::size_t SSIZE_0>
  static bool _free_ptr(VamPointer<T, SSIZE_0> &ptr) {
    DEBUG_VAMSPLT("released " << std::dec << ptr.size_bytes
                              << " B on NUMA node " << ptr.alloc_info->numa_node
                              << " at address 0x" << std::hex << ptr.start);

    if (ptr.alloc_info != nullptr) {
      if (ptr.alloc_info->ref_cnt.fetch_sub(1) == 1) {

        DEBUG_VAMSPLT("freed (really) "
                      << std::dec << ptr.alloc_info->size_bytes
                      << " B on NUMA node " << ptr.alloc_info->numa_node
                      << " at address 0x" << std::hex << ptr.alloc_info->data);

        if (ptr.alloc_info->data != nullptr)
          numa_free(ptr.alloc_info->data, ptr.alloc_info->size_bytes);
        delete ptr.alloc_info;
        return true;
      }
    }
    return false;
  }

  template <typename T, std::size_t SSIZE_0>
  static void _unset_attr(VamPointer<T, SSIZE_0> &ptr) {
    ptr.size_bytes = 0;
    ptr.start = nullptr;
    ptr.alloc_info = nullptr;
  }

  template <typename new_base_t>
  VamPointer<new_base_t, segment_size_bytes> _cast() const {
    VamPointer<new_base_t, segment_size_bytes> new_ptr;
    _copy_attr(*this, new_ptr);
    new_ptr.alloc_info->ref_cnt.fetch_add(1);

    DEBUG_VAMPTR("          casted ptr at pos 0x"
                 << std::hex << start << "; reference count " << std::dec
                 << ref_cnt->load());

    return new_ptr;
  }

public:
  // --------------------
  // --- CONSTRUCTORS ---
  // --------------------

  // Default constructor: creates an empty VamPointer
  VamPointer() : size_bytes(0), start(nullptr), alloc_info(nullptr) {
    DEBUG_VAMPTR("constructed empty ptr");
  }

  /**
   * @brief Construct a new Vam Pointer object
   *
   * @param size - size in **number of base_t elements**
   * @param numa_node - NUMA node to allocate memory on
   */
  VamPointer(std::size_t size, NumaId numa_node)
      : size_bytes(size * sizeof(base_t)) {
    void *raw_ptr = numa_alloc_onnode(size_bytes, numa_node);

    alloc_info = new AllocationInfo{numa_node, raw_ptr, size_bytes,
                                    std::atomic<uint32_t>(1)};
    start = reinterpret_cast<base_t *>(raw_ptr);

    DEBUG_VAMPPH("alloced " << std::dec << size_bytes << " B on NUMA node "
                            << numa_node << " at address 0x" << std::hex
                            << start);
    DEBUG_VAMPTR("     constructed ptr at pos 0x"
                 << std::hex << start << "; reference count " << std::dec
                 << alloc_info->ref_cnt.load());
  }

  // Copy constructor
  VamPointer(const this_t &other) {
    _copy_attr(other, *this);
    alloc_info->ref_cnt.fetch_add(1);

    DEBUG_VAMPTR("copy constructed ptr at pos 0x"
                 << std::hex << start << "; reference count " << std::dec
                 << alloc_info->ref_cnt.load());
  }

  // Move constructor
  VamPointer(this_t &&other) {
    _copy_attr(other, *this);
    _unset_attr(other);

    DEBUG_VAMPTR("move constructed ptr at pos 0x"
                 << std::hex << start << "; reference count " << std::dec
                 << alloc_info->ref_cnt.load());
  }

  // Destructor
  ~VamPointer() {
    // Case: empty pointer (alloc_info is nullptr)
    if (alloc_info == nullptr) {
      // empty pointers need no clean-up
      DEBUG_VAMPTR("destructed empty ptr");
      return;
    }

    // Case: valid pointer (alloc_info is not nullptr)
    DEBUG_VAMPTR("      destructed ptr at pos 0x"
                 << std::hex << start << "; reference count " << std::dec
                 << ref_cnt->load() - 1);

    if (alloc_info->ref_cnt.fetch_sub(1) == 1) {
      if (_free_ptr(*this))
        _unset_attr(*this);
    }
  }

  // ------------------------
  // --- ASSIGN OPERATORS ---
  // ------------------------

  // TODO make ref_cnt ++ as early as possible
  // Copy assignment operator
  VamPointer &operator=(const this_t &other) {
    if (this != &other) {
      // Reduce reference count of current data (if pointer is not empty ->
      // clean up previously held data)
      _free_ptr(*this);
      _copy_attr(other, *this);
      alloc_info->ref_cnt.fetch_add(1);
    }
    DEBUG_VAMPTR("copy assigned    ptr at pos 0x"
                 << std::hex << start << "; reference count " << std::dec
                 << alloc_info->ref_cnt.load());
    return *this;
  }

  // Move assignment operator
  VamPointer &operator=(this_t &&other) {
    if (this != &other) {
      // Reduce reference count of current data (if pointer is not empty ->
      // clean up previously held data)
      _free_ptr(*this);
      _copy_attr(other, *this);
      _unset_attr(other);
    }
    DEBUG_VAMPTR("move assigned    ptr at pos 0x"
                 << std::hex << start << "; reference count " << std::dec
                 << ref_cnt->load());
    return *this;
  }

  /**
   * @brief Splits the VamPointer into multiple slivers of approximately equal
   * size.
   * Each sliver will have a size that is a multiple of segment_size_bytes and
   * is alligned at segment boundaries.
   * @param sliver_count The number of slivers to split the VamPointer into.
   * @return std::vector<this_t> A vector containing the resulting slivers
   * (VamPointer).
   */
  std::vector<this_t> split(size_t sliver_count) {
    return split<segment_size_bytes>(sliver_count);
  }

  /**
   * @brief Splits the VamPointer into multiple slivers of approximately equal
   * size.
   * Each sliver will have a size that is a multiple of the specified segment
   * size and is alligned at segment boundaries.
   * @tparam __ignore Dummy template parameter for compability reasons.
   * @param sliver_count The number of slivers to split the VamPointer into.
   * @return std::vector<this_t> A vector containing the resulting slivers
   * (VamPointer).
   */
  template <std::size_t __ignore>
  std::vector<this_t> split(size_t sliver_count) {

    std::vector<this_t> slivers;
    slivers.reserve(sliver_count);

    size_t sliver_segments = segment_count() / sliver_count;
    size_t remainder = segment_count() % sliver_count;

    size_t offset = 0;
    for (size_t i = 0; i < sliver_count; ++i) {
      size_t sliver_segment_count =
          sliver_segments + (i < remainder ? 1 : 0); // distribute remainder

      slivers.emplace_back(*this);
      slivers.back().start =
          this->start + (offset * (segment_size_bytes / sizeof(base_t)));
      slivers.back().size_bytes = sliver_segment_count * segment_size_bytes;

      DEBUG_VAMSPLT("split: created sliver "
                    << std::dec << i << " at offset " << offset
                    << " segments, size " << slivers.back().size_bytes
                    << " B, start address 0x" << std::hex
                    << (void *)slivers.back().start);

      offset += sliver_segment_count;
    }

    return slivers;
  }

  // -----------------
  // --- ACCESSORS ---
  // -----------------

  /**
   * @brief Overloaded subscript operator for element access.
   * @param index The index of the element to access.
   * @return base_t& Reference to the element at the specified index.
   */
  base_t &operator[](std::size_t index) const {
    if (start == nullptr)
      throw std::logic_error("Error: Dereferencing an empty VamPointer");
    else if (index >= size())
      throw std::out_of_range(
          "Error: [VamPointer] Index " + std::to_string(index) +
          " out of bounds (size: " + std::to_string(size()) + ")");

    else
      return start[index];
  }

  /**
   * @brief Get a raw pointer to the data at the specified index.
   *
   * @param index The index of the element to get the pointer to.
   * @return base_t* Pointer to the element at the specified index.
   */
  base_t *data(std::size_t index) const {
    if (start == nullptr)
      throw std::logic_error("Error: Dereferencing an empty VamPointer");
    else if (index >= size())
      throw std::out_of_range(
          "Error: [VamPointer] Index " + std::to_string(index) +
          " out of bounds (size: " + std::to_string(size()) + ")");

    else
      return start + index;
  }

  /**
   * @brief Get the size of the VamPointer in number of elements.
   * @return std::size_t Size in number of base_t elements.
   */
  std::size_t size() const { return size_bytes / sizeof(base_t); }

  void manipulate_size(std::size_t new_size) {
    size_bytes = new_size * sizeof(base_t);
  }

  // here prefech?
  /**
   * @brief Get the segment at the specified segment index.
   *
   * @param index The index of the segment to retrieve.
   * @return std::tuple<base_t *, std::size_t> A tuple containing a pointer to
   * the segment and its size (element count)).
   */
  std::tuple<base_t *, std::size_t> get_segment(std::size_t index) const {
    if (start == nullptr)
      throw std::logic_error("Error: Dereferencing an empty VamPointer");
    else if (index >= segment_count())
      throw std::out_of_range("Error: [VamPointer] Segment index " +
                              std::to_string(index) +
                              std::string(" out of bounds (size: ") +
                              std::to_string(segment_count()) + ")");
    else {
      std::size_t segment_elements = segment_size_bytes / sizeof(base_t);
      std::size_t offset = index * segment_elements;
      std::size_t remaining_elements = size() - offset;

      return std::make_tuple(start + offset,
                             std::min(segment_elements, remaining_elements));
    }
  }

  /**
   * @brief Get the number of segments in the VamPointer.
   *
   * @return std::size_t Number of segments.
   */
  std::size_t segment_count() const {
    return (size_bytes + segment_size_bytes - 1) / segment_size_bytes;
  }

  // -------------
  // --- CASTS ---
  // -------------
  // it is allowed to cast any VamPointer<T, size> ot any VamPointer<U, size>
  // where T and U are integral types.

  operator VamPointer<uint8_t, segment_size_bytes>() const {
    return _cast<uint8_t>();
  }
  operator VamPointer<int8_t, segment_size_bytes>() const {
    return _cast<int8_t>();
  }
  operator VamPointer<uint16_t, segment_size_bytes>() const {
    return _cast<uint16_t>();
  }
  operator VamPointer<int16_t, segment_size_bytes>() const {
    return _cast<int16_t>();
  }
  operator VamPointer<uint32_t, segment_size_bytes>() const {
    return _cast<uint32_t>();
  }
  operator VamPointer<int32_t, segment_size_bytes>() const {
    return _cast<int32_t>();
  }
  operator VamPointer<uint64_t, segment_size_bytes>() const {
    return _cast<uint64_t>();
  }
  operator VamPointer<int64_t, segment_size_bytes>() const {
    return _cast<int64_t>();
  }
};

} // namespace vampir
