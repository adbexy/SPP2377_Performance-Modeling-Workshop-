#include "query.hpp"

class Section {
public:
    std::string name;
    size_t bytes;
    stop_watch<std::allocator<stop_watch_round>> & watch;
    double duration = -1.0;
    bool primary;
    static std::vector<Section> all_sections;

    Section (
        std::string name,
        size_t bytes,
        stop_watch<std::allocator<stop_watch_round>> & watch
    ) : name(name),
        bytes(bytes),
        watch(watch),
        primary(true)
    {
        watch.start_time();
    }
    Section (
        const Section & section
    ) : name(section.name),
        bytes(section.bytes),
        watch(section.watch),
        duration(section.duration),
        primary(false)
    {
    }

    ~Section () {
        if (primary) {
            watch.stop_time();
            duration = watch.get_cast_durations().back();
            all_sections.push_back(*this); // saves a !primary copy
        }
    }

    static void print() {
        std::cout << "Sections:" << std::endl;
        for (auto section : all_sections) {
            double throughput = (
                static_cast<double>(section.bytes) / (1ull << 30) / section.duration
            );
            printf(
                "section %20s: %12.8f s -> %8.3f GiB/s\n",
                section.name.c_str(),
                section.duration,
                throughput
            );
        }
    }
};
std::vector<Section> Section::all_sections;

/**
 * returns (
 *    your result (from your optimized implementation),
 *    the unoptimized, but reliable result,
 *    the time it took to run your implementation
 * )
 */
std::tuple<int64_t, int64_t, double> query(table_r &r, table_s &s) {

  // create intermediate buffers
  // #### MODIFY: feel free to adjust access patterns
  join_intermediate intermediate_join_buffer;
  intermediate_join_buffer.keys =
      vmalloc<uint32_t, 2048>(s.data_amount * 2, AccessPattern::LINEAR);
  intermediate_join_buffer.used =
      vmalloc<uint64_t, 4096>(s.data_amount * 2, AccessPattern::LINEAR);

  join_result join_res;
  join_res.positions =
      vmalloc<size_t, 4096>(r.fk.size(), AccessPattern::LINEAR);
  join_res.lengths = vmalloc<size_t, sizeof(size_t)>(r.fk.segment_count(),
                                                     AccessPattern::LINEAR);

  auto mat_offset = vmalloc<size_t, sizeof(size_t)>(r.fk.segment_count(),
                                                    AccessPattern::LINEAR);

  auto joint_a = vmalloc<int64_t, 4096>(r.data_amount, AccessPattern::LINEAR);
  auto joint_b = vmalloc<int64_t, 4096>(r.data_amount, AccessPattern::LINEAR);

  auto column_a_times_b =
      vmalloc<int64_t, 4096>(r.data_amount, AccessPattern::LINEAR);

  auto reduced_ab = vmalloc<int64_t, sizeof(int64_t)>(r.a.segment_count(),
                                                      AccessPattern::LINEAR);

  // #### end MODIFY

  uint32_t thread_count = 5;

  std::vector<std::pair<int, int>> pinning_ranges;
  #if TESTING
    pinning_ranges = Crobat::get_testing_pinning_ranges();
  #else
    pinning_ranges = Crobat::get_benchmarking_pinning_ranges();
  #endif

  ThreadManager tm(thread_pin_policy::automatic, pinning_ranges);

  // Create threads
  // #### MODIFY: adjust thread_count per thread group as needed
  // #### MODIFY: you may also change to ThreadManager pinning to manually and
  // pin
  // #### the threadgroups by hand (hard)
  tm.create_thread_group<true, false>(
      "prober_group", thread_count, probing, intermediate_join_buffer.keys,
      intermediate_join_buffer.used, SplitWrapper<0, typeof(r.fk)>(&r.fk),
      SplitWrapper<0, typeof(join_res.positions)>(&join_res.positions),
      SplitWrapper<0, typeof(join_res.lengths)>(&join_res.lengths));

  tm.create_thread_group<true, false>(
      "materialize_a", thread_count, materialize_position_list, joint_a,
      SplitWrapper<0, typeof(r.a)>(&r.a),
      SplitWrapper<0, typeof(join_res.positions)>(&join_res.positions),
      SplitWrapper<0, typeof(mat_offset)>(&mat_offset),
      SplitWrapper<0, typeof(join_res.lengths)>(&join_res.lengths));

  tm.create_thread_group<true, false>(
      "materialize_b", thread_count, materialize_position_list, joint_b,
      SplitWrapper<0, typeof(r.b)>(&r.b),
      SplitWrapper<0, typeof(join_res.positions)>(&join_res.positions),
      SplitWrapper<0, typeof(mat_offset)>(&mat_offset),
      SplitWrapper<0, typeof(join_res.lengths)>(&join_res.lengths));

  tm.create_thread_group<true, false>(
      "multiply", thread_count, multiply,
      SplitWrapper<0, typeof(column_a_times_b)>(&column_a_times_b),
      SplitWrapper<0, typeof(joint_a)>(&joint_a),
      SplitWrapper<0, typeof(joint_b)>(&joint_b));

  tm.create_thread_group<true, false>(
      "reduce_add", thread_count, reduce_add,
      SplitWrapper<0, typeof(reduced_ab)>(&reduced_ab),
      SplitWrapper<0, typeof(column_a_times_b)>(&column_a_times_b));

  // #### end MODIFY

  stop_watch<std::allocator<stop_watch_round>> query_stop_watch{
      clock_type::now(), 1};
  // stop query time (without thread creation and datageneration
  //    -> only compute throughput)
  { Section sec(
    "build_intermediate_join_buffer",
    3 * s.data_amount * sizeof(uint64_t),
    query_stop_watch
  );

  // build hastable single threaded, as it is hard to parallelize efficiently
  building(intermediate_join_buffer, s);

  } { Section sec(
    "prober_group",
    r.data_amount * sizeof(uint32_t) + 3 * s.data_amount * sizeof(uint64_t),
    query_stop_watch
  );
  tm.run({"prober_group"});

  }
  size_t offset = 0;
  { Section sec(
    "mat_offset",
    join_res.lengths.segment_count() * sizeof(size_t),
    query_stop_watch
  );
  // prepare offsets for materialization
  // (multiply is only possible with materialized columns)
  for (size_t i = 0; i < join_res.lengths.segment_count(); i++) {
    mat_offset[i] = offset;
    offset += std::get<0>(join_res.lengths.get_segment(i))[0];
  }

  } { Section sec("materialize_a_and_b",
    2 * (r.data_amount * sizeof(uint64_t) + (
        join_res.lengths.size() +
        join_res.positions.size() +
        join_res.lengths.segment_count()
    ) * sizeof(size_t)),
    query_stop_watch
  );
  tm.run({"materialize_a", "materialize_b"});

  } { Section sec("manipulate_size", 3 * sizeof(size_t), query_stop_watch);
  // adjus size of preallocated columns after materialization
  // (actual size is now known)
  joint_a.manipulate_size(offset);
  joint_b.manipulate_size(offset);
  column_a_times_b.manipulate_size(offset);

  } { Section sec("multiply",
    2 * r.data_amount * sizeof(uint64_t),
    query_stop_watch
  );
  tm.run({"multiply"});

  } { Section sec("reduce_add",
    r.data_amount * sizeof(uint64_t),
    query_stop_watch
  );
  tm.run({"reduce_add"});

  }
  int64_t final_sum = 0;
  { Section sec("final_sum",
      reduced_ab.segment_count() * sizeof(uint64_t),
      query_stop_watch
  );
  // finalize reduce add (add up partial sums from each segment)
  for (size_t i = 0; i < reduced_ab.segment_count(); i++) {
    auto [res_ptr, res_size] = reduced_ab.get_segment(i);
    final_sum += res_ptr[0];
  }

  }
  Section::print();
  tm.print_timings();

  double duration = query_stop_watch.get_duration_sum<std::chrono::seconds>();

  int64_t safe_sum = checksum(r, s);
  return std::make_tuple(final_sum, safe_sum, duration);
}

int main() {
  PageType ptype = K4_Normal;
  size_t data_amount = 1024 * 1024 * 128LL;
  size_t size_special_1 = 1024;
  size_t memory_amount = 2 * data_amount * sizeof(int64_t) +
                         data_amount * sizeof(uint32_t) +
                         size_special_1 * sizeof(uint32_t);

  // create tables
  // #### MODIFY: feel free to adjust access patterns
  auto r_a = vmalloc<int64_t, 4096>(data_amount, vampir::AccessPattern::LINEAR);
  auto r_b = vmalloc<int64_t, 4096>(data_amount, vampir::AccessPattern::LINEAR);

  auto r_fk =
      vmalloc<uint32_t, 2048>(data_amount, vampir::AccessPattern::LINEAR);
  auto s_pk =
      vmalloc<uint32_t, 2048>(size_special_1, vampir::AccessPattern::LINEAR);
  // #### end MODIFY

  // Generate data
  Datagenerator datagen;
  datagen.generate<int64_t>(r_a.data(0), r_a.size(), BASIC_UNIFORM, 1, 10000);
  datagen.generate<int64_t>(r_b.data(0), r_b.size(), BASIC_UNIFORM, 1, 10000);
  datagen.generate<uint32_t>(r_fk.data(0), r_fk.size(), BASIC_UNIFORM, 0,
                             size_special_1 * 3);

  datagen.generate<uint32_t>(s_pk.data(0), s_pk.size(), ID);

  // Assemble tables
  table_r r{r_a, r_b, r_fk, data_amount};
  table_s s{s_pk, size_special_1};

  // Run query
  const auto [fast_result, safe_result, seconds] = query(r, s);
  // Query finished

  const double throughput_Bps = memory_amount / seconds;

  std::cout << fast_result << std::endl
            << safe_result << std::endl
            << throughput_Bps << std::endl
            << throughput_Bps // the sescond throughput is for compatibility
            << std::endl;

  if (fast_result == safe_result)
    return 0;
  else {
    std::cerr << "Checksum and query result do not match!" << std::endl;
    return 1;
  }
}
// #### You may also modify the query however you want, as long as you do not
// #### change the semantics.
// #### (e.g. Copy data to a different buffer before processing, change the
// #### order of operations, change the segment size, etc.)
