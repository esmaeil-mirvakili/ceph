
#include "CoDel.h"

CoDel::CoDel(CephContext *_cct): timer(_cct, timer_lock){
    timer.init();
}

CoDel::~CoDel() {
    std::lock_guard l{timer_lock};
    timer.cancel_all_events();
    timer.shutdown();
}

void CoDel::initialize(int64_t init_interval, int64_t init_target){
    initial_interval = init_interval;
    interval = initial_interval;
    initial_target_latency = init_target;
    target_latency = initial_target_latency;
}

void CoDel::add_target_latency(int64_t size, int64_t target_latency_ns){
    target_latency_map[size] = target_latency_ns;
    min_latency_map[size] = INT_NULL;
}

void CoDel::register_queue_latency(int64_t latency, int64_t size) {
    if(!normalize_latency) {
        int64_t selected_size = INT_NULL;
        for (auto iter = min_latency_map.begin(); iter != min_latency_map.end(); ++iter)
            if(size < iter->first) {
                selected_size = iter->first;
                break;
            }
        if(selected_size != INT_NULL){
            if (min_latency_map[selected_size] == INT_NULL || latency < min_latency_map[selected_size]) {
                min_latency_map[selected_size] = latency;
            }
            return;
        }
    }
    if (min_latency == INT_NULL || latency < min_latency) {
        min_latency = latency;
        min_latency_txc_size = size;
    }
}

void CoDel::_interval_process() {
    if(_check_latency_violation()){
        // min latency violation
        violation_count++;
        _update_interval();
        on_min_latency_violation(); // handle the violation
    } else{
        // no latency violation
        violation_count = 0;
        interval = initial_interval;
        on_no_violation();
    }
    // reset interval
    min_latency = INT_NULL;
    min_latency_txc_size = 0;
    on_interval_finished();

    auto codel_ctx = new LambdaContext(
            [this](int r) {
                _interval_process();
            });
    auto interval_duration = std::chrono::nanoseconds(interval);
    timer.add_event_after(interval_duration, codel_ctx);
}

/**
* check if the min latency violate the target
* @return true if min latency violate the target, false otherwise
*/
bool CoDel::_check_latency_violation() {
    if(!normalize_latency){
        bool has_min_lat = false;
        for (auto iter = min_latency_map.begin(); iter != min_latency_map.end(); ++iter)
            if(iter->second != INT_NULL) {
                has_min_lat = true;
                if(iter->second > target_latency_map[iter->first]) {
                    return true;
                }
            }
        if(has_min_lat)
            return false;
    }
    if(min_latency != INT_NULL){
        int64_t selected_target_latency = target_latency;
        for (auto iter = target_latency_map.begin(); iter != target_latency_map.end(); ++iter)
            if(min_latency_txc_size < iter->first) {
                selected_target_latency = iter->second;
                break;
            }
        if(normalize_latency){
            int64_t normalized = min_latency - selected_target_latency;
            if(normalized <= 0) {
                normalized = min_latency;
            }
            if (normalized > target_latency)
                return true;
        }else
            if(min_latency > selected_target_latency)
                return true;
    }
    return false;
}

void CoDel::_update_interval() {
    auto sqrt = (int) std::round(std::sqrt(violation_count));
    interval = initial_interval / sqrt;
    if(interval <= 0){
        interval = 1000;
    }
}

/**
* reset the algorithm
*/
void CoDel::reset() {
    interval = initial_interval;
    target_latency = initial_target_latency;
    min_latency = INT_NULL;
    std::lock_guard l{timer_lock};
    timer.cancel_all_events();
    std::cout << "target init:" << initial_target_latency << std::endl;
    std::cout << "target:" << target_latency << std::endl;
    std::cout << "interval init:" << initial_interval << std::endl;
    std::cout << "interval:" << interval << std::endl;
}
