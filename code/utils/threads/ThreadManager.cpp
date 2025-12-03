#include "ThreadManager.hpp"

//(De-)Constructors:

ThreadManager::ThreadManager(thread_pin_policy pin_policy = thread_pin_policy::manually, std::vector<std::pair<int, int>> pin_range = {}) :
        pin_policy(pin_policy), pin_range(pin_range) {
        
    bool pin_range_empty = true;
    for(auto& range : pin_range) {
        if(range.first <= range.second) {
            pin_range_empty = false;
            break;
        }
    }
    assert(!(pin_policy == thread_pin_policy::automatic && pin_range_empty) && "Pin range must be provided when using automatic pinning policy");

    thread_groups = std::map<std::string, ThreadGroup*>();
    reset();
}

ThreadManager::~ThreadManager() {
    for(auto& group : thread_groups) {
        delete group.second;
    }
}

// Private member functions:
    
void ThreadManager::reset() {
    for(auto & group : thread_groups) {
        delete group.second; // call destructor to clean up
    }
    thread_groups.clear();
    for(auto & pinnings : thread_pinnings) {
        pinnings.second.clear();
    }
    thread_pinnings.clear();
    next_core_index = 0;
}

// Public member functions:

//Template functions are to be defined in the header file, so they can be instantiated with different types.
//template<class F, class... Args>
//void ThreadManager::create_thread_group(std::string group_id, uint32_t thread_count, F&& func, Args&&... args);

void ThreadManager::run(std::vector<std::string> group_ids){

    std::vector<ThreadWrapper *> join_threads;

    for(std::string& group_id : group_ids) {
        //might throw an exception if group_id does not exist
        auto *group_join_thread = thread_groups.at(group_id)->run_async();
        
        join_threads.push_back(group_join_thread);
        
    }
    
    for(ThreadWrapper* thread : join_threads) {
        thread->join();
    }
}

std::vector<ThreadWrapper *> ThreadManager::run_async(std::vector<std::string> group_ids){

    std::vector<ThreadWrapper *> join_threads;

    for(std::string& group_id : group_ids) {
        //might throw an exception if group_id does not exist
        auto *group_join_thread = thread_groups.at(group_id)->run_async();
        
        join_threads.push_back(group_join_thread);
        
    }
    
    return join_threads;
}
