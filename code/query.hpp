#include "allocator.hpp"
#include "generator.hpp"
#include <cstdint>
#include <tuple>
#include <unordered_map>

#include <iostream>
#include <ostream>

#include "algorithms/dbops/arithmetic/arithmetic.hpp"
#include "algorithms/dbops/filter/filter.hpp"
#include "algorithms/dbops/join/hash_join.hpp"
#include "algorithms/dbops/join/hash_semi_join_simd_linear_probing.hpp"
#include "algorithms/dbops/materialize/materialize.hpp"
#include "threads/ThreadManager.hpp"
#include "vmalloc/VamPointer.hpp"
#include "vmalloc/vmalloc.hpp"
#include <chrono>

using namespace vampir;
using namespace tuddbs;

struct table_r {
  VamPointer<int64_t, 4096> a;
  VamPointer<int64_t, 4096> b;
  VamPointer<uint32_t, 2048> fk;
  size_t data_amount;
};

struct table_s {
  VamPointer<uint32_t, 2048> pk;
  size_t data_amount;
};

struct join_intermediate {
  VamPointer<uint32_t, 2048> keys;
  VamPointer<uint64_t, 4096> used;
};

struct join_result {
  VamPointer<size_t, 4096> positions;
  VamPointer<size_t, sizeof(size_t)> lengths;
};

int64_t checksum(table_r &r, table_s &s) {
  // original compute code for validation
  std::unordered_map<uint32_t, bool> map;
  map.reserve(s.pk.size());
  for (size_t j = 0; j < s.pk.size(); j++) {
    map[s.pk[j]] = 1;
  }
  int64_t safe_sum = 0;
  for (size_t i = 0; i < r.a.size(); i++) {
    safe_sum += (r.a[i] * r.b[i]) * map[r.fk[i]];
  }
  return safe_sum;
}

void materialize_position_list(VamPointer<int64_t, 4096> result,
                               VamPointer<int64_t, 4096> data,
                               VamPointer<size_t, 4096> positions,
                               VamPointer<size_t, sizeof(size_t)> offset,
                               VamPointer<size_t, sizeof(size_t)> size) {

  Materialize<tsl::simd<int64_t, tsl::avx512>,
              OperatorHintSet<hints::intermediate::position_list>>
      mat;
  for (size_t i = 0; i < positions.segment_count(); i++) {
    auto [size_ptr, size_size] = size.get_segment(i);
    auto [offset_ptr, offset_size] = offset.get_segment(i);
    auto [pos_ptr, pos_size] = positions.get_segment(i);
    auto [data_ptr, data_size] = data.get_segment(i);

    mat(result.data(offset_ptr[0]), data_ptr, data_ptr + data_size, pos_ptr,
        size_ptr[0]);
  }
}

void building(join_intermediate &ji, table_s &right_side) {
  using join_t = tuddbs::Hash_Semi_Join_RightSide_SIMD_Linear_Probing<
      tsl::simd<uint32_t, tsl::avx512>, size_t>;

  auto [key_ptr, key_size] = ji.keys.get_segment(0);
  auto [used_ptr, used_size] = ji.used.get_segment(0);
  join_t::builder_t builder(key_ptr, used_ptr, ji.keys.size(), ji.used.size());

  for (size_t i = 0; i < right_side.pk.segment_count(); i++) {
    auto [ptr, size] = right_side.pk.get_segment(i);
    builder(ptr, size);
  }
}

void probing(VamPointer<uint32_t, 2048> keys, VamPointer<uint64_t, 4096> used,
             VamPointer<uint32_t, 2048> fk, VamPointer<size_t, 4096> positions,
             VamPointer<size_t, sizeof(size_t)> lengths) {
  using join_t = tuddbs::Hash_Semi_Join_RightSide_SIMD_Linear_Probing<
      tsl::simd<uint32_t, tsl::avx512>, size_t>;

  auto [key_ptr, key_size] = keys.get_segment(0);
  auto [used_ptr, used_size] = used.get_segment(0);
  join_t::prober_t prober(key_ptr, used_ptr, used.size());

  for (size_t i = 0; i < fk.segment_count(); i++) {
    auto [ptr, size] = fk.get_segment(i);
    auto [pos_ptr, pos_size] = positions.get_segment(i);
    auto [len_ptr, len_size] = lengths.get_segment(i);
    len_ptr[0] = prober(pos_ptr, ptr, size);
  }
}

void multiply(VamPointer<int64_t, 4096> result, VamPointer<int64_t, 4096> col_a,
              VamPointer<int64_t, 4096> col_b) {

  col_multiplier_t<tsl::simd<int64_t, tsl::avx512>> multiplier;

  for (size_t i = 0; i < col_a.segment_count(); i++) {
    auto [a_ptr, a_size] = col_a.get_segment(i);
    auto [b_ptr, b_size] = col_b.get_segment(i);
    auto [res_ptr, res_size] = result.get_segment(i);

    multiplier(res_ptr, a_ptr, a_size, b_ptr);
  }
}

void reduce_add(VamPointer<int64_t, sizeof(int64_t)> result,
                VamPointer<int64_t, 4096> data) {

  col_sum_t<tsl::simd<int64_t, tsl::avx512>> reducer;

  for (size_t i = 0; i < data.segment_count(); i++) {
    auto [data_ptr, data_size] = data.get_segment(i);
    auto [res_ptr, res_size] = result.get_segment(i);

    reducer(res_ptr, data_ptr, data_size);
  }
}
