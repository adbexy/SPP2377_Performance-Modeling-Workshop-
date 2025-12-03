/**
 * @file stop_watch.hpp
 * @author André Berthold
 * @brief Adds a stop watch that stops the time, how long somthing (between
 * start and stop) runs.
 * @version 0.1
 * @date 2024-12-17
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <cassert>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <ratio>
#include <vector>

using clock_type = std::chrono::steady_clock;
using time_point = std::chrono::time_point<clock_type>;
struct stop_watch_round {
  time_point start = time_point::min();
  time_point end = time_point::min();
};

inline time_point epoch_now() { return clock_type::now(); }

/// @brief Policies for handling consecutive start/stop calls.
enum class double_call_policy {
  forbidden,     ///< fail/assert on consecutive start/stop call
  save_earliest, ///< keep earliest start/stop call timestamp, ignore later ones
  save_latest    ///< overwrite start/stop timestamp wit later ones/latest one
};

/// @brief This policy forbids multiple consecutive start/stop calls without
/// the corresponding stop/start call in between. If such a call occures, an
/// assertion fails.
constexpr int FORBIDDEN = 0;
/// @brief This policy saves the time of the first start/stop call and ignores
/// all consecutive calls until the corresponding stop/start call is issued.
constexpr int SAVE_EARLIEST = 1;
/// @brief This policy saves the time of the last start/stop call and overwrites
/// all previous calls that did not have a corresponding stop/start call in
/// between.
constexpr int SAVE_LATEST = 2;

/// @brief Class that allows to construct stop_watch objects that can be used
/// for single threaded time measurements (for multithreaded use, multiple
/// instances of this class should be created with the same epoch)
/// @tparam ALLOC - Allocator of the rounds array, to select an allcation
/// routine (to e.g. select the numa node)
/// @tparam AUTO_EXPAND - Determines if (true) the rounds array should be
/// expanded automatically when the space is depleted (may increas latency of
/// start calls, which could lead to unpleasant side effects for e.g. nested
/// time measurements). Or if (false) it should not be expanded automatically,
/// in that case the user responsible to expand the rounds array if it runs out
/// of space. Otherwise out of memory errors (incl. segmentations faults) are
/// possible!
/// @tparam DOUBLE_START_POLICY - Determines how two (or more) consecutive
/// start calls should be handled. See decumentation of defined constant above.
/// Alowed values are FORBIDDEN, SAVE_EARLIEST aSAVE_LATESTAST.
/// @tparam DOUBLE_STOP_POLICY - Determines how two (or more) consecutive
/// stop calls should be handled. See decumentation of defined constant above.
/// Alowed values are FORBIDDEN, SAVE_EARLIEST aSAVE_LATESTAST.
template <class ALLOC, bool AUTO_EXPAND = true,
          double_call_policy START_POLICY = double_call_policy::forbidden,
          double_call_policy STOP_POLICY = double_call_policy::forbidden>
class stop_watch {
  using round = stop_watch_round;

private:
  std::vector<round, ALLOC> rounds;
  size_t current;
  bool taking_time;
  const size_t expand_size;

public:
  const time_point epoch;

private:
  void expand(size_t size) { rounds.resize(rounds.size() + size); }

public:
  /// @brief Calculates a double representing the given duration in the time
  /// unit TO_UNIT with granularity GRANULARITY.
  /// @tparam TO_UNIT - unit of the returned double
  /// @tparam GRANULARITY - granularity of the cast value
  /// @tparam D - class of the duration type
  /// @param duration duration to cast to double
  /// @return double, represents duration in unit TO_UNIT
  template <class TO_UNIT, class GRANULARITY, class D>
  static constexpr inline double cast(D duration) {
    using ratio = std::ratio_divide<typename GRANULARITY::period,
                                    typename TO_UNIT::period>;

    long tmp = std::chrono::duration_cast<GRANULARITY>(duration).count();
    double res = static_cast<double>(tmp * ratio::num) / ratio::den;
    return res;
  }

public:
  stop_watch() = delete;

  /// @brief Creates a stop_watch object that can be used to measure run time
  /// of certain routines.
  /// @param epoch - Reference time_point, that can be used to compare
  /// multiple instances of stop watch.
  /// @param init_round_size - initial size of the rounds array, to avoid
  /// indroduced latency due to creation of new round structs when start call
  /// is issued
  stop_watch(time_point epoch = clock_type::now(),
             size_t init_round_size = 1000)
      : epoch(epoch), expand_size(init_round_size) {
    rounds.resize(init_round_size);
    current = 0;
    taking_time = false;
  }

  /// @brief Starts the watch that tracts time till correspopnding stop call.
  /// If IGNORE_DOUBLE_START is set and two consecutive calls to this function
  /// without an stop call in between occures, the time between last call of
  /// this function and consecutive stop call is measured. Otherwise, an
  /// assertion ensures every measurement is terminated by a stop call,
  /// before the next start call.
  /// If AUTO_EXPAND is set, the function automatically expands the rounds
  /// array by its initial size (init_round_size) if its space is depleted.
  void start_time() {
    if constexpr (START_POLICY == double_call_policy::forbidden) {
      // when fail on doulbe start (so a start call while the previous start
      // call had no finisching stop call) is active this assertion would fail
      // in that case
      assert(!taking_time);

      // otherwise pretend the previous start did not happen -> simpley
      // overwrite the start value
    } else if constexpr (START_POLICY == double_call_policy::save_earliest) {
      if (taking_time) {
        return;
      }
    } else if constexpr (START_POLICY == double_call_policy::save_latest) {
      if (taking_time) {
        rounds[current].start = clock_type::now();
        return;
      }
    }

    if constexpr (AUTO_EXPAND) {
      if (current == rounds.size()) {
        expand(expand_size);
      }
    }

    // take time last -> everything done after would count to the measured time
    taking_time = true;
    rounds[current].start = clock_type::now();
  }

  /// @brief Stops the watch that terminates the previously started measurement.
  /// If IGNORE_DOUBLE_STOP is set and stop call occures before a
  /// corresponding start call, the previous measurements end time is set to
  /// the current time, or the call is ignore if no previous measurement was
  /// completed so far. Otherwise, an assertion ensures that a measurement is
  /// runnning when a stop call is issued.
  void stop_time() {
    // take time first -> everything done before would count to the measured
    // time
    time_point end = clock_type::now();

    if constexpr (STOP_POLICY == double_call_policy::forbidden) {
      // when fail on stop before (corresponding) start is active
      // this assertion would fail in that case
      assert(taking_time);
    } else if constexpr (STOP_POLICY == double_call_policy::save_earliest) {
      // if its not active -> ignore the call
      if (!taking_time) {
        return;
      }
    } else if constexpr (STOP_POLICY == double_call_policy::save_latest) {
      // if its not active -> remove old round and pretend the previous stop did
      // not happen
      if (!taking_time) {
        if (current > 0) {
          rounds[current - 1].end = end;
          taking_time = false;
        }
        return;
      }
    }

    rounds[current].end = end;
    taking_time = false;
    ++current;
  }

  /// @brief Expands the number of reserved fields in the rounds vector by
  /// size.
  /// @param size Number of elements to default initialize in the vector
  /// @return void
  void expand_rounds(size_t size) { expand(size); }

  /// @brief Expands the number of reserved fields in the rounds vector by
  /// size if less the an min_free elements are available.
  /// @param min_free Threshold of unused reserved elements, that decideds if
  /// new elements schould be created or not
  /// @param size Number of elements to expand the vector by (default: min_free)
  /// @return wether the rounds vector was expanded or not
  bool expand_rounds_if(size_t min_free, size_t size) {
    if (rounds.size() - current <= min_free) {
      expand_rounds(size);
      return true;
    } else {
      return false;
    }
  }

  bool expand_rounds_if(size_t size) {
    if (rounds.size() - current <= size) {
      expand_rounds(size);
      return true;
    } else {
      return false;
    }
  }

  /// @brief Returns the number of unused reserved elements in the rounds
  /// vector.
  /// @return number of unused reserved elements in the rounds vector
  size_t unused_size() { return rounds.size() - current; }

  // handy functions

  /// @brief Returns the subsequence of completed rounds from the rounds array,
  /// which contains a structs with time_point start and time_point end
  /// @return The array containing the timepoint, when start and end was
  /// triggered
  std::vector<round, ALLOC> get_rounds() {
    std::vector result(rounds);
    result.resize(current);
    return result;
  }

  // maybe specify concept narrow down the valid template parameter

  /// @brief Returns an vector, which contains pairs of start and end time
  /// (double), which are casted to the specified UNIT at a granularity of
  /// GRANULARITY.
  /// @tparam UNIT - time unit of the doubles in the returned vector
  /// @tparam GRANULARITY - granularity of the casted value
  /// @return vector, which contains pairs of start and end time (double)
  template <class UNIT = std::chrono::seconds,
            class GRANULARITY = std::chrono::nanoseconds>
  std::vector<std::pair<double, double>> get_cast_rounds() {
    auto comp_rnds = get_rounds();
    std::vector<std::pair<double, double>> result;
    result.reserve(comp_rnds.size());
    for (round rnd : comp_rnds) {
      result.emplace_back(
          std::make_pair(cast<UNIT, GRANULARITY>(rnd.start - epoch),
                         cast<UNIT, GRANULARITY>(rnd.end - epoch)));
    }
    return result;
  }

  /// @brief Returns an vector, which contains durations equal to the
  /// differnce between end and start of a round in the rounds array
  /// @return vector, which contains durations of the rounds
  std::vector<std::common_type<time_point::duration, time_point::duration>>
  get_durations() {
    auto comp_rnds = get_rounds();
    std::vector<std::common_type<time_point::duration, time_point::duration>>
        result(comp_rnds.size());
    for (size_t i = 0; i < result.size(); ++i) {
      result[i] = comp_rnds[i].end - comp_rnds[i].start;
    }
    return result;
  }

  /// @brief Returns an vector, which contains durations (double) equal to the
  /// differnce between end and start of a round in the rounds array, which are
  /// casted to the specified UNIT at a granularity of GRANULARITY.
  /// @tparam UNIT - time unit of the doubles in the returned vector
  /// @tparam GRANULARITY - granularity of the casted value
  /// @return vector, which contains durations (doubles) in the specified time
  /// UNIT
  template <class UNIT = std::chrono::seconds,
            class GRANULARITY = std::chrono::nanoseconds>
  std::vector<double> get_cast_durations() {
    auto comp_rnds = get_rounds();
    std::vector<double> result(comp_rnds.size());
    for (size_t i = 0; i < result.size(); ++i) {
      result[i] =
          cast<UNIT, GRANULARITY>(comp_rnds[i].end - comp_rnds[i].start);
    }
    return result;
  }

  /// @brief Returns the sum of all durations (double), which is casted to the
  /// specified UNIT at a granularity of GRANULARITY.
  /// @tparam UNIT - time unit of the returned sum
  /// @tparam GRANULARITY - granularity of the casted value
  /// @return double, the duration sum in UNIT
  template <class UNIT = std::chrono::seconds,
            class GRANULARITY = std::chrono::nanoseconds>
  double get_duration_sum() {
    double sum = 0.0;
    for (double n : get_cast_durations<UNIT, GRANULARITY>()) {
      sum += n;
    }
    return sum;
  }

  /// @brief Returns the average of all durations (double), which is casted to
  /// the specified UNIT at a granularity of GRANULARITY.
  /// @tparam UNIT - time unit of the returned average
  /// @tparam GRANULARITY - granularity of the casted value
  /// @return double, the duration average in UNIT
  template <class UNIT = std::chrono::seconds,
            class GRANULARITY = std::chrono::nanoseconds>
  double get_duration_avg() {
    double sum = get_duration_sum<UNIT, GRANULARITY>();
    return sum / current;
  }

  /// @brief Returns the min of all durations (double), which is casted to the
  /// specified UNIT at a granularity of GRANULARITY.
  /// @tparam UNIT - time unit of the returned average
  /// @tparam GRANULARITY - granularity of the casted value
  /// @return double, the duration average in UNIT
  template <class UNIT = std::chrono::seconds,
            class GRANULARITY = std::chrono::nanoseconds>
  double get_duration_min() {
    double min = 1000000.0;
    for (double n : get_cast_durations<UNIT, GRANULARITY>()) {
      if (n < min)
        min = n;
    }
    return min;
  }
  /// @brief Returns the max of all durations (double), which is casted to the
  /// specified UNIT at a granularity of GRANULARITY.
  /// @tparam UNIT - time unit of the returned average
  /// @tparam GRANULARITY - granularity of the casted value
  /// @return double, the duration average in UNIT
  template <class UNIT = std::chrono::seconds,
            class GRANULARITY = std::chrono::nanoseconds>
  double get_duration_max() {
    double max = 0.0;
    for (double n : get_cast_durations<UNIT, GRANULARITY>()) {
      if (n > max)
        max = n;
    }
    return max;
  }
};

template <class ALLOC, bool AUTO_EXPAND = true,
          double_call_policy START_POLICY = double_call_policy::forbidden,
          double_call_policy STOP_POLICY = double_call_policy::forbidden>
class concurrent_stop_watch
    : public stop_watch<ALLOC, AUTO_EXPAND, START_POLICY, STOP_POLICY> {
private:
  std::mutex mtx;

public:
  concurrent_stop_watch(time_point epoch = clock_type::now(),
                        size_t init_round_size = 1000)
      : stop_watch<ALLOC, AUTO_EXPAND, START_POLICY, STOP_POLICY>(
            epoch, init_round_size) {}

  void start_time() {
    std::lock_guard<std::mutex> lock(mtx);
    stop_watch<ALLOC, AUTO_EXPAND, START_POLICY, STOP_POLICY>::start_time();
  }
  void stop_time() {
    std::lock_guard<std::mutex> lock(mtx);
    stop_watch<ALLOC, AUTO_EXPAND, START_POLICY, STOP_POLICY>::stop_time();
  }
};
