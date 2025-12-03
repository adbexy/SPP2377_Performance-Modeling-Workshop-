#include "ThreadGroup.hpp"


ThreadGroup::~ThreadGroup() {
    if(!start_pistol_fired) start_pistol.set_value(); // signal all threads to start

    for(auto& thread_wrapper : thread_pool) {
        // destructor of ThreadWarpper joins them as well
        delete thread_wrapper;
    }
    thread_pool.clear();
}

void ThreadGroup::run() {
    
    start_pistol.set_value();

    for(ThreadWrapper* thread : thread_pool) {
        thread->join(); // join all threads in the group
    }
}

ThreadWrapper * ThreadGroup::run_async() {
    if(start_pistol_fired) {
        throw std::runtime_error("ThreadGroup already started");
    }
    start_pistol.set_value();
    start_pistol_fired = true;

    // return the last thread, which join the other thread of the thread group and can be joined later
    return thread_pool[thread_count - 1];
}

std::vector<int> ThreadGroup::pin_threads(std::vector<std::pair<int, int>> range, uint64_t start_core_index) {
    std::vector<int> result;
    for(auto thread : thread_pool) {
        result.push_back(thread->pin_thread(range, start_core_index));
    }
    return result;
}
