#pragma once

#include <cassert>
#include <cstdint>
#include <future>
#include <map>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "../stop_watch.hpp"

#include "ThreadGroup.hpp"
#include "ThreadWrapper.hpp"

enum class thread_pin_policy {
  manually, ///< No pinning of threads to CPU cores or by used via
            ///< ThreadManager::pin_threads_for_group
  automatic ///< Pin threads in order of the given CPU ID ranges.
};

// TODO optional round count for stop_watch preallocation
/// @brief Class managing multiple thread groups, allowing their creation,
/// execution, and timing.
class ThreadManager {
private:
  /// @brief Map storing thread groups identified by their unique string IDs.
  std::map<std::string, ThreadGroup *> thread_groups;

  /// @brief Epoch time point used for initializing timers in thread groups.
  const time_point epoch = epoch_now();

  /// @brief Policy for pinning threads to CPU cores.
  const thread_pin_policy pin_policy;
  /// @brief Range of CPU cores to be used for pinning threads.
  const std::vector<std::pair<int, int>> pin_range;
  /// @brief Index of the next core to pin a thread to, used when pinning in
  /// order.
  uint64_t next_core_index = 0;
  /// @brief Map storing the pinning information for each thread group by their
  /// IDs.
  std::map<std::string, std::vector<int>> thread_pinnings;

private:
  /// @brief Resets the ThreadManager by clearing all existing thread groups.
  /// This function deletes all thread groups and clears the internal map.
  void reset();

public:
  /// @brief Constructor for the ThreadManager class.
  ThreadManager(thread_pin_policy pin_policy,
                std::vector<std::pair<int, int>> pin_range);
  ThreadManager(const ThreadManager &) = delete; // disable copy constructor
  ~ThreadManager();

  /// @brief Creates a new thread group with the specified parameters and adds
  /// it to the manager.
  /// @tparam MEASURE_GROUP Whether to measure per group time.
  /// @tparam MEASURE_THREAD Whether to measure per thread time.
  /// @tparam F Function type of the function to be eccetuten in the threads.
  /// @tparam ...Args Argument types of the function to be executed in the
  /// threads.
  /// @param group_id Unique identifier for the thread group.
  /// @param thread_count Number of threads in the group.
  /// @param func Function to be executed by the threads in the group.
  /// @param args Arguments to be passed to the function.
  /// @throws std::invalid_argument if thread_count is zero.
  /// @throws std::runtime_error if a thread group with the same ID already
  /// exists.
  template <bool MEASURE_GROUP, bool MEASURE_THREAD, class F, class... Args>
  void create_thread_group(std::string group_id, uint32_t thread_count,
                           F &&func, Args &&...args) {
    if (thread_count == 0) {
      throw std::invalid_argument("Thread count must be greater than 0");
    }
    if (thread_groups.find(group_id) != thread_groups.end()) {
      throw std::runtime_error("Thread group with ID " + group_id +
                               " already exists");
    }

    ThreadGroup *group = new ThreadGroup(group_id, thread_count, epoch);
    group->initialize<MEASURE_GROUP, MEASURE_THREAD>(
        epoch, std::forward<F>(func), std::forward<Args>(args)...);
    if (pin_policy == thread_pin_policy::automatic) {
      auto pinnings = group->pin_threads(pin_range, next_core_index);
      thread_pinnings.emplace(group_id, pinnings);
      next_core_index += thread_count;
    }

    thread_groups.emplace(group_id, group);
  }

  /// @brief Runs the specified thread groups by their IDs. This function starts
  /// all threads in the specified groups and waits for their completion.
  /// @throws std::out_of_range if any of the specified group IDs do not exist??
  /// @param group_ids Vector of string IDs representing the thread groups to be
  /// run.
  void run(std::vector<std::string> group_ids);

  std::vector<ThreadWrapper *> run_async(std::vector<std::string> group_ids);

  /// @brief Pins the threads of a specific thread group to specific CPU cores
  /// within the given ranges.
  /// @param group_id ID of the thread group whose threads are to be pinned.
  /// @param range A vector of pairs, where each pair represents a range of CPU
  /// cores to pin a thread to.
  /// @return A vector of integers representing the CPU cores each thread is
  /// pinned to.
  std::vector<int>
  pin_threads_for_group(const std::string &group_id,
                        std::vector<std::pair<int, int>> range) {
    auto pinnings = thread_groups.at(group_id)->pin_threads(range);
    thread_pinnings.emplace(group_id, pinnings);
    return pinnings;
  }

  /// @brief Prints the timing results for all thread groups and their
  /// individual threads.
  void print_timings() {
    for (auto &[id, group] : thread_groups) {
      std::cout << "Group " << id << " timing: ";
      if (group->group_timer_valid) {
        std::cout
            << group->group_timer.get_duration_sum<std::chrono::milliseconds>()
            << " ms";
      } else
        std::cout << "not measured";
      std::cout << std::endl;

      if (group->thread_timers.size() == 0)
        std::cout << "  No thread timings available" << std::endl;
      else
        for (int i = 0; i < group->thread_timers.size(); ++i) {
          std::cout << "  Thread " << i << " timing: ";

          if (group->thread_timers_valid) {
            std::cout << group->thread_timers[i]
                             .get_duration_sum<std::chrono::milliseconds>()
                      << " ms";
          } else
            std::cout << "not measured";
          std::cout << std::endl;
        }
    }
  }

  double sum_group_durations() {
    double total_duration = 0.0;
    for (auto &[id, group] : thread_groups) {
      if (group->group_timer_valid) {
        total_duration = group->group_timer.get_duration_sum();
      }
    }
    return total_duration;
  }

  void print_thread_pinnings() {
    for (auto &[group_id, pinnings] : thread_pinnings) {
      std::cout << "Thread group " << group_id << " ("
                << thread_groups.at(group_id)->thread_count << ") pinnings: ";
      for (auto core_id : pinnings) {
        std::cout << core_id << " ";
      }
      std::cout << std::endl;
    }
  }
};
