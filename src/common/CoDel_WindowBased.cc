
#include "CoDel_WindowBased.h"

CoDel::CoDel(CephContext *_cct){
}

CoDel::~CoDel() {
}

void CoDel::initialize(int64_t init_window_size, int64_t init_target){
    initial_window_size = init_window_size;
    window_size = init_window_size;
    initial_target_latency = init_target;
    target_latency = initial_target_latency;
}

void CoDel::register_queue_latency(int64_t latency, int64_t size) {
    if (min_latency == INT_NULL || latency < min_latency) {
        min_latency = latency;
    }
    processed_size += size;
    if(processed_size > window_size)
        _interval_process();
}

void CoDel::_interval_process() {
    if(_check_latency_violation()){
        // min latency violation
        violation_count++;
//        _update_interval();
        on_min_latency_violation(); // handle the violation
    } else{
        // no latency violation
        violation_count = 0;
//        interval = initial_interval;
        on_no_violation();
    }
    // reset interval
    min_latency = INT_NULL;
    processed_size = 0;
    on_interval_finished();
}

/**
* check if the min latency violate the target
* @return true if min latency violate the target, false otherwise
*/
bool CoDel::_check_latency_violation() {
    if(min_latency != INT_NULL){
        if(min_latency > target_latency)
            return true;
    }
    return false;
}

void CoDel::_update_interval() {
    auto sqrt = (int) std::round(std::sqrt(violation_count));
//    interval = initial_interval / sqrt;
//    if(interval <= 0){
//        interval = 1000;
//    }
}

/**
* reset the algorithm
*/
void CoDel::reset() {
    window_size = initial_window_size;
    target_latency = initial_target_latency;
    min_latency = INT_NULL;
    std::cout << "target init:" << initial_target_latency << std::endl;
    std::cout << "target:" << target_latency << std::endl;
    std::cout << "window init:" << initial_window_size << std::endl;
    std::cout << "window:" << window_size << std::endl;
}
