#pragma once

#include <concepts>
#include <cstdint>
#include <vector>

// describe, what an Allocator needs to implement
/*template<class T>
concept Splitable = requires(T value, std::size_t n)
{
    { value.split<1>(n) } -> std::same_as<std::vector<T>>;
};*/

/// @brief Helper function to test the Splitable concept. This function accepts a vector of any type T.
/// Thus it can be used to check if a type T has a member function `split` that returns a `std::vector<T>`.
/// @tparam T Type to be checked.
/// @param  vector Vector of type T.
template <class T>
void splitTester(std::vector<T>) {
	return;
}

/// @brief Concept defining the requirements for a type to be considered "splitable".
/// A type is considered splitable if it has a member function `split` that takes a
/// `std::size_t` parameter and returns a `std::vector` of the same type.
/// @tparam T Type to be checked for the Splitable concept.
template <class T>
concept Splitable = requires(T value, std::size_t n) {
	{ splitTester(value.split(n)) };
};

/// @brief Wrapper for splitable types that marks wrapped values to be splitted among threads.
/// This struct is used to indicate that a specific argument to a thread function should be split
/// into chunks for each thread. The wrapped type must satisfy the Splitable concept, meaning it
/// must have a member function `split` that takes a `std::size_t` parameter and returns a `std::vector` of the same
/// type.
/// @tparam blk_size Block size to be used for splitting (chunk size is a multiple of the block size).
/// @tparam T Type to be wrapped. The type must satisfy the Splitable concept.
template <std::size_t blk_size, Splitable T>
struct SplitWrapper {
	static constexpr std::size_t block_size = blk_size;
	T *value;

	SplitWrapper(T *value) : value(value) {};
};
