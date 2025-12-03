#pragma once

#include <iostream>


#include <cstdint>
#include <future>
#include <thread>
#include <tuple>
#include <vector>

#include "stop_watch.hpp"
#include "../cpu_set_utils.hpp"

// Every thread within a thread group should start and stop this timer on its own to measure the total execution time 
// of the group (from start of erarliest thread to end of latest thread).
// Alternatively one thread could start and stop the timer for the whole group, but this method would include the time to join the threads.
using group_timer_t = concurrent_stop_watch<std::allocator<struct stop_watch_round>, false, double_call_policy::save_earliest, double_call_policy::save_latest>;
using thread_timer_t = stop_watch<std::allocator<struct stop_watch_round>, false, 
        double_call_policy::forbidden, double_call_policy::forbidden>;

/// @brief Wrapper for thread function arguments.
/// @tparam F Function type.
/// @tparam ...Args Argument types.
template<class F, class ... Args>
struct ThreadArgs {
    F func;
    static constexpr uint32_t argn = sizeof...(Args);
    std::tuple<Args...> args;

    ThreadArgs(F&& f, Args&&... a) : func(std::forward<F>(f)), args(std::make_tuple(std::forward<Args>(a)...)) {}

    ThreadArgs(F&& f, std::tuple<Args...>& t) : func(std::forward<F>(f)), args(t) {}
};

/// @brief Wrapper class for std::thread that holds additional thread information allows to pass additional arguments
/// and to pin the thread to a specific core. It implements the thread function that waits for a start signal,
/// starts and stops optional timers, calls the function to benchmark, and joins the thread from the thread group down to one thread.
class ThreadWrapper {
private:
    /// @brief Reference to the thread pool of the thread group this ThreadWrapper belongs to.
    std::vector<ThreadWrapper *> &thread_pool;
    /// @brief ID of this thread within its thread group.
    uint32_t thread_id;
    /// @brief Total number of threads within the thread group this ThreadWrapper belongs to.
    uint32_t thread_count;
    /// @brief Future that is set when all threads of the thread group should start execution.
    std::shared_future<void> start_fut;
    /// @brief The actual thread object.
    std::thread thread;

    /// @brief Reference to the group timer of the thread group this ThreadWrapper belongs to.
    group_timer_t &group_timer;
    /// @brief Reference to the thread timer of this ThreadWrappers thread.
    thread_timer_t &thread_timer;

public:
    /// @brief Constructor for the ThreadWrapper class.
    /// @param thread_pool Reference to the thread pool of the thread group this ThreadWrapper belongs to.
    /// @param thread_id ID of this thread within its thread group.
    /// @param thread_count Total number of threads within the thread group this ThreadWrapper belongs to.
    /// @param start_fut Future that is set when all threads of the thread group should start execution.
    /// @param group_timer Reference to the group timer of the thread group this ThreadWrapper belongs to.
    /// @param thread_timer Reference to the thread timer of this ThreadWrappers thread.
    ThreadWrapper(std::vector<ThreadWrapper *> &thread_pool, uint32_t thread_id, uint32_t thread_count, std::shared_future<void> start_fut, group_timer_t &group_timer, thread_timer_t &thread_timer)
            : thread_pool(thread_pool), thread_id(thread_id), thread_count(thread_count), 
              start_fut(start_fut), group_timer(group_timer), thread_timer(thread_timer) {};

    /// @brief Initializes the thread wrapper with the given function and arguments. This is done in a separate 
    /// function as contructors do not allow explicit template parameters.
    /// @tparam F Function type.
    /// @tparam ...Args Argument types.
    /// @tparam MEASURE_GROUP Whether to measure group time.
    /// @tparam MEASURE_THREAD Whether to measure thread time.
    /// @param func Function to be executed by the thread.
    /// @param args Arguments to be passed to the function.
    template<bool MEASURE_GROUP, bool MEASURE_THREAD, class F, class ... Args>
    void initalize(F&& func, std::tuple<Args...>& args) {
        // create the thread and passes this object as well to access its members.
        thread = std::thread(thread_func<MEASURE_GROUP, MEASURE_THREAD, F, Args...>, this, 
            ThreadArgs<F, Args...>(std::forward<F>(func), args));
    }

    ThreadWrapper() = delete;
    ThreadWrapper(const ThreadWrapper&) = delete; // disable copy constructor
    ThreadWrapper& operator=(const ThreadWrapper&) = delete; // disable copy assignment
    ~ThreadWrapper();

    /// @brief Pins the thread to a specific core within the given range of cpu_ids.
    /// @param range Vector of pairs representing the range of CPU IDs to pin the thread to.
    /// @return 0 on success, -1 on failure.
    int pin_thread(std::vector<std::pair<int, int>> range, uint64_t start_core_index);

    /// @brief Joins the thread wrapped by this ThreadWrapper, blocking until it finishes execution.
    void join();

private:
    /// @brief Function executed by the thread. It waits for the start signal, starts and stops optional timers,
    /// calls the function to benchmark, and joins the thread from the thread group down to one thread.
    /// @tparam F Function type.
    /// @tparam ...Args Argument types.
    /// @tparam MEASURE_GROUP Whether to measure group time.
    /// @tparam MEASURE_THREAD Whether to measure thread time.
    /// @param thisObject Pointer to the ThreadWrapper object.
    /// @param args Arguments to be passed to the function.
    template<bool MEASURE_GROUP, bool MEASURE_THREAD, class F, class ... Args>
    static void thread_func(ThreadWrapper *thisObject, ThreadArgs<F, Args...> args) {

        thisObject->start_fut.wait();

        if constexpr (MEASURE_GROUP) thisObject->group_timer.start_time();
            
        if constexpr (MEASURE_THREAD) thisObject->thread_timer.start_time();
        
        std::apply(args.func, args.args);
        
        if constexpr (MEASURE_THREAD) thisObject->thread_timer.stop_time();

        if constexpr (MEASURE_GROUP) thisObject->group_timer.stop_time();

        thisObject->_simple_join();
        
    }

    /// @brief (WIP) Joins all threads of the thread group in a tree-like manner, so that the number of join calls is reduced.)
    void _join();
    /// @brief Joins all threads of the thread group by the last thread of the group.
    void _simple_join();
};
