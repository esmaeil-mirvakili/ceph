
#include "CoDel.h"

CoDel::CoDel(CephContext *_cct): timer(_cct, timer_lock){
timer.init();
std::lock_guard l{timer_lock};
_interval_process();
}

CoDel::~CoDel() {
    std::lock_guard l{timer_lock};
    timer.cancel_all_events();
    timer.shutdown();
}

void CoDel::register_queue_latency(int64_t latency) {
    if(min_latency == INT_NULL || latency < min_latency){
        min_latency = latency;
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
    on_interval_finished();

    codel_ctx = new LambdaContext(
            [this](int r) {
                interval_process();
            });git
    auto interval_duration = std::chrono::nanoseconds(interval);
    timer.add_event_after(interval_duration, codel_ctx);
}

/**
* check if the min latency violate the target
* @return true if min latency violate the target, false otherwise
*/
bool CoDel::_check_latency_violation() {
    return min_latency != INT_NULL && min_latency > target_latency;
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
    _interval_process();
    std::cout << "target init:" << initial_target_latency << std::endl;
    std::cout << "target:" << target_latency << std::endl;
    std::cout << "interval init:" << initial_interval << std::endl;
    std::cout << "interval:" << interval << std::endl;
}
