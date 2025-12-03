#include "ThreadWrapper.hpp"

ThreadWrapper::~ThreadWrapper() {
    if(thread.joinable()) thread.join();
}

int ThreadWrapper::pin_thread(std::vector<std::pair<int, int>> range, uint64_t start_core_index) {
    return pin_thread_in_range(thread, start_core_index + thread_id, range);
}

void ThreadWrapper::join() {
    thread.join();
}

void ThreadWrapper::_join() {
    uint32_t iteration = 0b1;
    while(thread_id % (iteration*2) == 0) {
        if(thread_id + iteration < thread_count) {
            std::cout << "T " << thread_id << " joining with T " << thread_id + iteration << std::endl;
            // join with next still active (not-joined) thread
            thread_pool[thread_id + iteration]->join();
            iteration *= 2;
        } else break;// to break the loop for thread_id 0.
    }
}

void ThreadWrapper::_simple_join() {
    //the last thread is responssible for joining all other thread of its thread group
    if(thread_id == thread_count - 1) {
        for(int i = 0; i < thread_count - 1; ++i) {
            thread_pool[i]->join();
        }
    }
}