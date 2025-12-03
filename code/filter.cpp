
#include "algorithms/dbops/filter/filter.hpp"
#include "vmalloc/VamPointer.hpp"
#include "vmalloc/vmalloc.hpp"

using namespace vampir;
int main() {
  const int element_count = 32;

  using base_t = uint16_t;
  using SIMDStyle = tsl::simd<base_t, tsl::avx2>;

  auto input = vmalloc<base_t>(element_count, vampir::AccessPattern::LINEAR);
  auto output =
      vmalloc<base_t, 128>(element_count, vampir::AccessPattern::LINEAR);

  for (int i = 0; i < element_count; ++i) {
    input[i] = i % 32;
    output[i] = 0;
  }

  // intended behaviour: one byte only contains the filter result of
  // SimdStyle::vector_element_count() elements
  tuddbs::Filter_LT<SIMDStyle> filter_op(12);

  for (size_t i = 0; i < input.segment_count(); ++i) {
    auto [in_ptr, in_size] = input.get_segment(i);
    auto [out_ptr, out_size] = output.get_segment(i);
    // std::cout << std::bitset<sizeof(base_t) * 8>(out_ptr[0]) << std::endl;
    filter_op(out_ptr, in_ptr, in_size);
    std::cout << std::bitset<sizeof(base_t) * 8>(out_ptr[0]) << std::endl;
    std::cout << std::bitset<sizeof(base_t) * 8>(out_ptr[1]) << std::endl;
  }

  int i = 0;
  size_t bit_pos = 0;
  size_t byte_pos = 0;
  auto output_byte = static_cast<VamPointer<uint8_t, 128>>(output);
  for (int i = 0; i < element_count; ++i, ++bit_pos) {
    bool pred_result = (input[i] < 12);

    if (bit_pos >= std::min((size_t)8, SIMDStyle::vector_element_count())) {
      bit_pos = 0;
      ++byte_pos;
    }

    bool output_result = (output_byte[byte_pos] & (1 << bit_pos)) != 0;

    if (pred_result != output_result) {
      std::cerr << "Error at position " << i << ": expected " << pred_result
                << ", got " << output_result << std::endl;
    }
  }
}
