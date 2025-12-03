#pragma once

#include <cstdint>
#include <thread>
#include <cassert>
#include <iostream>
#include <sstream>
#include <vector>
#include <utility>

/** Sets all bits in a given cpu_set_t between L and H (condition L <= H)*/
#define CPU_BETWEEN(L, H, SET) assert(L <= H); for(; L < H; ++L) {CPU_SET(L, SET);}

namespace cpu_id {
	using subrange_t            = std::pair<int, int>;
	using subrange_directed_t   = std::tuple<int, int, bool>; // bool true means reversed

	using range_t               = std::vector<subrange_t>;
	using range_directed_t      = std::vector<subrange_directed_t>;

	// abstract typename for both subrange_t and subrange_directed_t
	template <typename T>
	concept SubRange = (
		std::is_same<T, subrange_t>::value ||
		std::is_same<T, subrange_directed_t>::value
	);

	// abstract typename. void function(Range auto range) {...} takes either type of range.
	template <typename T>
	concept Range = (
		std::is_same<T, range_t>::value ||
		std::is_same<T, range_directed_t>::value
	);

	int start(const SubRange auto& subrange) {
		return std::get<0>(subrange);
	}
	int end(const SubRange auto& subrange) {
		return std::get<1>(subrange);
	}
	int length(const SubRange auto& subrange) {
		return end(subrange) - start(subrange);
	}
	inline bool reversed(const subrange_t& subrange) {
		return false;
	}
	inline bool reversed(const subrange_directed_t& subrange) {
		return std::get<2>(subrange);
	}

	std::string printable(const Range auto& range) {
		std::stringstream result;
		for (const auto& subrange : range) {
			if (reversed(subrange)) {
				result << "(" << end(subrange) << ".." << start(subrange) << "] ";
			} else {
				result << "[" << start(subrange) << ".." << end(subrange) << ") ";
			}
		}
		return result.str();
	}
};

/**
 * Applies the affinity defined in set to the thread, through pthread library
 * calls. If it fails it wites the problem to stderr and terminated the program.
*/
inline void pin_thread(std::thread& thread, cpu_set_t* set) {
    int error_code = pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), set);
    if (error_code != 0) {
        std::cerr << "Error calling pthread_setaffinity_np in copy_pool assignment: " << error_code << std::endl;
        exit(-1);
    }
}

/**
 * Returns the cpu id of the thread_id-th cpu in a given (multi)range.
 * If the relevant sub-range is specified as reversed,
 * the giving of ids inside is started from the back. A thread_id
 * greater than the number of cpus in the (multi)range are valid. In this case
 * the (thread_id % #cpus in the range)-th cpu in the range is returned.
*/
inline int get_cpu_id(int thread_id, const cpu_id::Range auto& range) {
    int subrange_size = cpu_id::length(range[0]);

    int i = 0;
    while(subrange_size <= thread_id) {
        thread_id -= subrange_size;
        i = (i + 1) % range.size();
        subrange_size = cpu_id::length(range[i]);
    }
	if (cpu_id::reversed(range[i])) {
		thread_id = subrange_size - 1 - thread_id;
	}
    return thread_id + cpu_id::start(range[i]);
}

/*inline void cpu_set_between(cpu_set_t* set, uint32_t low, uint32_t high) {
    assert(low != high);
    if (low > high) std::swap(low, high);

    for(; low < high; ++low) {
        CPU_SET(low, set);
    }
}*/

/**
 * Pins the given thread to the thread_id-th cpu in the given range.
 * returns the cpu id where the thread was pinned.
*/
inline int pin_thread_in_range(std::thread& thread, int thread_id, const cpu_id::Range auto& range) {
    cpu_set_t set;
    CPU_ZERO(&set);
	int id = get_cpu_id(thread_id, range);
    CPU_SET(id, &set);

    pin_thread(thread, &set);

	return id;
}

/**
 * Pins the given thread to the given cpu_id.
 * returns the cpu id where the thread was pinned.
*/
inline int pin_thread_to_cpu_id(std::thread& thread, int cpu_id) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu_id, &set);

    pin_thread(thread, &set);

	return cpu_id;
}

/**
 * Pins the given thread to all cpus in the given range.
*/
inline void pin_thread_in_range(std::thread& thread, const cpu_id::Range auto& range) {
    cpu_set_t set;
    CPU_ZERO(&set);
    for(auto r : range) { CPU_BETWEEN(cpu_id::start(r), cpu_id::end(r), &set); }

    pin_thread(thread, &set);
}

/**
 * Pins the given thread to all cpu ids between low (incl.) and high (excl.).
*/
inline void pin_thread_between(std::thread& thread, uint32_t low, uint32_t high) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_BETWEEN(low, high, &set);

    pin_thread(thread, &set);
}

/**
 * create pinning ranges for crobat (XeonMAX)
 */
namespace Crobat {
	static constexpr uint64_t cpus_per_node = 12;
	static constexpr uint64_t exec_nodes_count = 8;

	static constexpr
	std::vector<std::pair<int, int>>
	get_pinning_ranges (int exec_node) {
		std::vector<std::pair<int, int>> result;
		for (int hyperthreads = 0; hyperthreads <= 1; hyperthreads++) {
			// hyperthreads of same physical cpus as first few ids.
			// this layout is not observed on all architectures!
			const uint64_t node_number = hyperthreads * exec_nodes_count + exec_node;
			result.emplace_back(
				cpus_per_node *  node_number,
				cpus_per_node * (node_number + 1)
			);
		}
		return result;
	}

	static constexpr
	std::vector<std::pair<int, int>>
	get_testing_pinning_ranges () {
		std::vector<std::pair<int, int>> result;
		for (int exec_node = 0; exec_node <= 3; exec_node++) {
			for (int hyperthreads = 0; hyperthreads <= 1; hyperthreads++) {
				// hyperthreads of same physical cpus as first few ids.
				// this layout is not observed on all architectures!
				const uint64_t node_number = hyperthreads * exec_nodes_count + exec_node;
				result.emplace_back(
					cpus_per_node *  node_number,
					cpus_per_node * (node_number + 1)
				);
			}
		}
		return result;
	}

	static constexpr
	std::vector<std::pair<int, int>>
	get_benchmarking_pinning_ranges () {
		std::vector<std::pair<int, int>> result;
		for (int exec_node = 4; exec_node <= 7; exec_node++) {
			for (int hyperthreads = 0; hyperthreads <= 1; hyperthreads++) {
				// hyperthreads of same physical cpus as first few ids.
				// this layout is not observed on all architectures!
				const uint64_t node_number = hyperthreads * exec_nodes_count + exec_node;
				result.emplace_back(
					cpus_per_node *  node_number,
					cpus_per_node * (node_number + 1)
				);
			}
		}
		return result;
	}
}

