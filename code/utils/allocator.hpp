#pragma once

#include <cstdint>
#include <vector>
#include <cstdlib>

#ifdef LINUX
    #include <sys/mman.h>
    #include <page-info.h>
#endif

#ifdef LIBNUMA
    #include <numa.h>
#endif


#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)
#define MAP_HUGE_1GB    (30 << MAP_HUGE_SHIFT)

enum PageType {
    K4_Normal,
    M2_HugePages,
    G1_HugePages,
    Transparent_HugePages
};

template<typename T>
void print_page_info(T *array, size_t length) {
    #ifdef LINUX
    constexpr int KPF_THP = 22;
    page_info_array pinfo = get_info_for_range(array, array + length);
    flag_count thp_count = get_flag_count(pinfo, KPF_THP);
    if (thp_count.pages_available) {
        std::cout << "\033[32m";
        std::cout << "Source pages allocated with transparent hugepages: "
            << 100.0 * thp_count.pages_set / thp_count.pages_total
            << "% (" << thp_count.pages_total
            << " pages, " << 100.0 * thp_count.pages_available / thp_count.pages_total
            << "% flagged)\033[0m" << std::endl;
        // std::cout << "\033[0m" << std::flush;
    } else {
        std::cout << "\033[31mCouldn't determine hugepage info \033[31;1m(you are probably not running as root)\033[0m" << std::endl;
    }
    #endif
}
template<typename T>
class AlignedAllocator {
public:
    using value_type = T;

    explicit AlignedAllocator(const PageType ptype) : _ptype(ptype) {}

    AlignedAllocator(const AlignedAllocator&) = default;
    AlignedAllocator(AlignedAllocator&&) = default;

    AlignedAllocator& operator=(const AlignedAllocator&) noexcept = default;
    AlignedAllocator& operator=(AlignedAllocator&&) noexcept = default;

    T* allocate(const std::size_t count) {

        const auto size = count < 4096? sizeof(T) * 4096: sizeof(T) * count;

        #ifdef LINUX
        if (_ptype == M2_HugePages) {
            return static_cast<T*> (::mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_2MB, -1, 0));
        }
        if(_ptype == G1_HugePages) {
            return static_cast<T*> (::mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB, -1, 0));
        }
        #endif

        T* data = static_cast<T*>(std::aligned_alloc(4096U, size));
        #ifdef LINUX
        if(_ptype == Transparent_HugePages){
            madvise(data, size, MADV_HUGEPAGE);
        }
        #endif

        return data;
    }


    T* allocate(const std::size_t count, const std::size_t numa_node ) {
        const auto size = sizeof(T) * count;

        #ifdef LINUX
        if (_ptype == M2_HugePages) {
            return static_cast<T*> (::mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_2MB, -1, 0));
        }
        if(_ptype == G1_HugePages) {
            return static_cast<T*> (::mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB, -1, 0));
        }
        #endif

        T* data = nullptr;

        #ifdef LIBNUMA
        if(numa_available() != -1){
            if(numa_node < numa_max_possible_node()){
                data = static_cast<T*>(numa_alloc_onnode(size, numa_node));
            }else{
                std::cout << "\033[31;1mError:\033[22;m the system only supports " << numa_max_possible_node() << " numa nodes.\033[0m" << std::endl;
                std::cout << "\033[33m       Using aligned alloc instead\033[0m" << std::endl;
            }
        }
        #endif


        if(data == nullptr){
            data = static_cast<T*>(std::aligned_alloc(4096U, size));
        }

        #ifdef LINUX
        if(_ptype == Transparent_HugePages){
            madvise(data, size, MADV_HUGEPAGE);
        }
        #endif
        return data;
    }


    void deallocate(T* ptr, const std::size_t count) {
        #ifdef LINUX
        if (_ptype == M2_HugePages || _ptype == G1_HugePages) {
            ::munmap(ptr, sizeof(T) * count);
        }
        else {
            std::free(ptr);
        }
        #else
        std::free(ptr);
        #endif
    }

private:
    PageType _ptype = K4_Normal;
};