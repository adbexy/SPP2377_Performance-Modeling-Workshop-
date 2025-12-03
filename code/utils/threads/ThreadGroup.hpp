#pragma once

#include <cstdint>
#include <future>
#include <map>
#include <string>
#include <thread>
#include <tuple>
#include <vector>
#include <type_traits>
#include <functional>

#include "stop_watch.hpp"
#include "ThreadWrapper.hpp"
#include "SplitWrapper.hpp"

/// @brief Class representing a group of threads that can be managed (startet/stopped) together and carry out the same 
/// function (on different data).
class ThreadGroup {
    private:
        /// @brief Flag indicating whether the start signal has been sent to the threads.
        bool start_pistol_fired = false;
        /// @brief Promise used to signal the threads to start execution.
        std::promise<void> start_pistol;
        /// @brief Shared future that threads wait on before starting execution.
        std::shared_future<void> start_fut;

        /// @brief Pool of thread wrappers representing the threads in the group.
        std::vector<ThreadWrapper *> thread_pool;
        
        
    public:
        /// @brief Identifier for the thread group.
        std::string group_id;
        /// @brief Number of threads in the group.
        uint32_t thread_count;

        /// @brief Flag indicating whether group timing is enabled (used when writing the results to a file).
        bool group_timer_valid = false; 
        /// @brief Group timer to measure the total execution time of the thread group.
        group_timer_t group_timer;
        /// @brief Flag indicating whether per-thread timing is enabled (used when writing the results to a file).
        bool thread_timers_valid = false; 
        /// @brief Vector of timers to measure the execution time of individual threads.
        std::vector<thread_timer_t> thread_timers;

    public:
        /// @brief Constructor for the ThreadGroup class.
        /// @param group_id Identifier for the thread group.
        /// @param thread_count Number of threads in the group.
        /// @param timer_epoch Epoch time point for initializing the timers. This is typically the same for all thread groups in a ThreadManager.
        ThreadGroup(std::string group_id, uint32_t thread_count, time_point timer_epoch)
                 : group_id(group_id), thread_count(thread_count), group_timer(timer_epoch) {} 
                 // the epoch is set here even if its no decideable by the constructor weather or not the timer is used, 
                 // but the epoch is const and has to be set at construction time (the timer is implicitly constructed with this constructor call)

        /// @brief Initializes the thread group with the given function and arguments. This is done in a separate 
        /// function as contructors do not allow explicit template parameters.
        /// @tparam MEASURE_GROUP Whether to measure group time.
        /// @tparam MEASURE_THREAD Whether to measure thread time.
        /// @tparam F Function type.
        /// @tparam ...Args Argument types.
        /// @param timer_epoch Epoch time point for initializing the timers. This is typically the same for all thread groups in a ThreadManager.
        /// @param func Function to be executed by the threads.
        /// @param args Arguments to be passed to the function.
        template<bool MEASURE_GROUP, bool MEASURE_THREAD, class F, class... Args>
        void initialize(time_point timer_epoch, F&& func, Args&&... args) {
            // setup timers if needed
            if constexpr(MEASURE_GROUP) {
                group_timer_valid = true;
            } if constexpr(MEASURE_THREAD) { 
                thread_timers_valid = true;
                thread_timers.reserve(thread_count);
                for(int i = 0; i < thread_count; ++i) 
                    thread_timers.emplace_back(thread_timer_t(timer_epoch));
            }
                
            // setup start signaling mechanism
            start_pistol = std::promise<void>();
            start_fut = start_pistol.get_future().share();
            
            // setup ThreadWrappers
            // generate_thread_arg_lists creates a vector of tuples, each tuple containing the arguments for one thread.
            // Here each argument is either the original argument, or split up into chunks (if it was wrapped in a 
            // SplitWrapper).
            auto parameters = generate_thread_arg_lists(thread_count, std::make_tuple(args...));
            for(int i = 0; i < thread_count; ++i) {
                ThreadWrapper* thread_wrapper = new ThreadWrapper(thread_pool, i, thread_count, 
                        start_fut, group_timer, thread_timers[i]);
                thread_wrapper->initalize<MEASURE_GROUP, MEASURE_THREAD>
                        (std::forward<F>(func), parameters[i]);

                thread_pool.push_back(thread_wrapper);
            }
        }

        ~ThreadGroup();

        /// @brief Runs the thread group by signaling all threads to start and then waits for their completion.
        /// This function blocks until all threads in the group have finished execution.
        void run();
        /// @brief Runs the thread group by signaling all threads to start and returns immediately. The caller is 
        /// responsible for joining the returned ThreadWrapper later.
        /// @return Pointer to the ThreadWrapper of the last thread in the group (which joins the other threads in its 
        /// thread group), which can be joined later.
        ThreadWrapper * run_async();

        /// @brief Pins the threads in the group to specific CPU cores within the given ranges.
        /// @param range A vector of pairs, where each pair represents a range of CPU cores to pin a thread to.
        /// @return A vector of integers representing the CPU cores each thread is pinned to.
        std::vector<int> pin_threads(std::vector<std::pair<int, int>> range, uint64_t start_core_index = 0);

    private:
        /// @brief Generates a vector of tuples, where each tuple contains the arguments for one thread.
        /// Each argument is either the original argument or a subchunk of the argument if it was wrapped in a SplitWrapper.
        /// @tparam ...Args Types of the arguments.
        /// @param thread_count Number of threads (size of the resulting vector).
        /// @param unified_args Tuple containing the original arguments (some of which may be wrapped in SplitWrappers).
        /// @return A vector of tuples, where each tuple contains the arguments for one thread.
        template<class... Args>
        auto generate_thread_arg_lists(uint32_t thread_count, std::tuple<Args...> unified_args) {

            // a tuple of vectors, that contain the split arguments for each thread (they are split if they were wrapped in a SplitWrapper)
            auto split_arg_lists = 
                    _split_all(thread_count, unified_args, std::make_index_sequence<sizeof...(Args)>());

            // transposes the split_arg_list into a vector of tuples (that can be used to construct an ThreadWrapper)
            auto list_of_argument_lists = _transpose(thread_count, split_arg_lists);

            return list_of_argument_lists;
        }

        /// @brief Transposes a tuple of vectors into a vector of tuples. This is used to convert the split arguments
        /// for each thread from a tuple of vectors (where each vector contains the split values for one argument) into
        /// a vector of tuples (where each tuple contains the arguments for one thread).
        /// @tparam ...Args Types of the arguments.
        /// @param thread_count Number of threads (size of the resulting vector).
        /// @param values Tuple containing vectors of values for each argument.
        /// @return A vector of tuples, where each tuple contains the arguments for one thread.
        template<class ... Args>
        std::vector<std::tuple<Args...>> _transpose(uint32_t thread_count, std::tuple<std::vector<Args>...> values) {
            std::vector<std::tuple<Args...>> result;
            for(uint32_t t = 0; t < thread_count; ++t) {
                result.push_back(_gen_tuple(t, values, std::make_index_sequence<sizeof...(Args)>()));
            }
            return result;
        }

        /// @brief Generates a tuple of values for a specific thread from a tuple of vectors.
        /// @tparam Tuple Type of the input tuple containing vectors.
        /// @tparam ...IndexSeq Parameter pack representing the indices of the tuple elements.
        /// @param t Index of the thread for which to generate the tuple of values.
        /// @param tuple Tuple containing vectors of values for each argument.
        /// @param std::index_sequence<IndexSeq...> Unnamed parameter used to deduce the IndexSeq template parameter pack.
        /// @return A tuple of values for the thread with index t.
        template<typename Tuple, std::size_t ... IndexSeq>
        // the unnamed std::index_sequence determines the value of the IndexSeq parameter pack
        auto _gen_tuple(uint32_t t, Tuple& tuple, std::index_sequence<IndexSeq...>) {
            // the below parameter pack expansion pattern expands the whole expression
            // 'get_value_at(std::get<IndexSeq>(tuple), t)'; i.e. for each parameter in IndexSeq one expression 
            // (replacing IndexSeq with the specific value) is produced
            return std::make_tuple(get_value_at(std::get<IndexSeq>(tuple), t)...);
        }

        /// @brief Helper function to get the value at a specific index from a vector or return the value itself if it's not a vector.
        /// This function is used to extract the appropriate argument for each thread from the split arguments.
        /// @tparam T Type of the value or vector.
        /// @param vector Vector from which to get the value (if T is a vector).
        /// @param index Index of the value to get (if T is a vector).
        /// @return The value at the specified index if T is a vector, or the last value of the vector if the index is out of bounds. If T is not a vector, returns the value itself.
        template<typename T>
        T& get_value_at(std::vector<T>& vector, size_t index) {
            return vector[std::min(index, vector.size())];
        }

        /// @brief Overload of get_value_at for non-vector types. Simply returns the value itself.
        /// @tparam T Type of the value.
        /// @param value Value to return.
        /// @param index Unused parameter (present for signature compatibility).
        /// @return The value itself.
        template<typename T>
        T get_value_at(T& value, size_t index) {return value;}
        

        /// @brief Helper function to split all arguments in a tuple. This function uses template
        /// metaprogramming to iterate over the tuple and apply the split_splitable function to
        /// each element.
        /// @tparam Tuple Type of the tuple containing the arguments.
        /// @tparam ...IndexSeq Parameter pack representing the indices of the tuple elements.
        /// @param t Number of threads to split the arguments for.
        /// @param tuple Tuple containing the arguments to be split.
        /// @param std::index_sequence<IndexSeq...> Unnamed parameter used to deduce the IndexSeq template parameter pack.
        /// @return A tuple of vectors containing the "split" arguments for each thread.
        template<typename Tuple, std::size_t ... IndexSeq>
        // we cannot use Args... here as a parameter pack is only allowed at the end of a template parameter list
        // the unnamed std::index_sequence determines the value of the IndexSeq parameter pack
        auto _split_all(uint32_t t, Tuple& tuple, std::index_sequence<IndexSeq...>) {
            // the below parameter pack expansion pattern expands the whole expression
            // 'split_splitable(std::get<IndexSeq>(tuple))'; i.e. for each parameter in IndexSeq one expression 
            // (replacing IndexSeq with the specific value) is produced
            return std::make_tuple(split_splitable(t, std::get<IndexSeq>(tuple))...);
        }

        /// @brief Splits a thread parameter into thread_count chunks. This 
        /// function overlaod is used for values wrappen into a SplitWrapper 
        /// (those parameter that can be splitted and should be splited). 
        /// @tparam T Type of the value to be split (must implement the Splitable concept -> ensured by SplitWrapper).
        /// @tparam block_size Block size to be used for splitting (chunk size is a multiple of the block size).
        /// @param thread_count Number of threads to split the value for. 
        /// @param value Value to be split (wrapped in a SplitWrapper).
        /// @return A vector of splitted values, one for each thread.
        template<class T, std::size_t block_size>
        inline auto split_splitable(uint32_t thread_count, SplitWrapper<block_size, T>& value) {
            return value.value->template split<block_size>(thread_count);
        }
        
        /// @brief Splits a thread parameter into thread_count chunks. This 
        /// function overload is used for values that are not wrappen into a 
        /// SplitWrapper (those parameter that cannot or should not be 
        /// splitted). Those values are simply replicated for each thread.
        /// @tparam T Type of the value to be split.
        /// @param thread_count Number of threads to split the value for.
        /// @param value Value to be replicated (or "split").
        /// @return A vector containing the original value replicated for each thread.
        template<class T>
        inline std::vector<T> split_splitable(uint32_t thread_count, T& value) {
            // we need to prepare one value for each thread, otherwise, due to 
            // reference shinanigains bad things would happen making the 
            // address sanitizer to cry -> Heap Overflow.
            return std::vector<T>(thread_count, value);
        }

};
