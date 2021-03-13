
#include "CoDel_Window.h"

CoDel::CoDel(CephContext *_cct): timer(_cct, timer_lock){
    timer.init();
}

CoDel::~CoDel() {
    std::lock_guard l{timer_lock};
    timer.cancel_all_events();
    timer.shutdown();
}

void CoDel::register_queue_latency(int64_t latency, int64_t size) {
    if(submitted_size + size > window_size){
        _process_latencies();
    }
    if(min_latency == INT_NULL || latency < min_latency){
        min_latency = latency;
    }
    submitted_size += size;
}

void CoDel::_timeout_process() {
    _process_latencies();
}

void CoDel::_process_latencies() {
    std::lock_guard l{timer_lock};
    timer.cancel_all_events();

    if(_check_latency_violation()){
        // min latency violation
        violation_count++;
        _update_window();
        on_min_latency_violation(); // handle the violation
    } else{
        // no latency violation
        violation_count = 0;
        window_size = initial_window_size;
        on_no_violation();
    }
    // reset interval
    min_latency = INT_NULL;
    submitted_size = 0;
    on_interval_finished();

    auto codel_ctx = new LambdaContext(
            [this](int r) {
                _timeout_process();
            });
    auto timeout_duration = std::chrono::nanoseconds(timeout);
    timer.add_event_after(timeout_duration, codel_ctx);
}

/**
* check if the min latency violate the target
* @return true if min latency violate the target, false otherwise
*/
bool CoDel::_check_latency_violation() {
    return min_latency != INT_NULL && min_latency > target_latency;
}

void CoDel::_update_window() {
    auto sqrt = (int) std::round(std::sqrt(violation_count));
    window_size = initial_window_size / sqrt;
    if(window_size <= 0){
        window_size = 1024;
    }
}

/**
* reset the algorithm
*/
void CoDel::reset() {
    window_size = initial_window_size;
    target_latency = initial_target_latency;
    min_latency = INT_NULL;
    std::lock_guard l{timer_lock};
    timer.cancel_all_events();
    std::cout << "target init:" << initial_target_latency << std::endl;
    std::cout << "target:" << target_latency << std::endl;
    std::cout << "window size init:" << initial_window_size << std::endl;
    std::cout << "window size:" << window_size << std::endl;
}
