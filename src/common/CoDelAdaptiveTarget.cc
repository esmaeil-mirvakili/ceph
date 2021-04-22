
#include "CoDelAdaptiveTarget.h"

CoDel::CoDel(CephContext *_cct): timer(_cct, timer_lock){
    timer.init();
}

CoDel::~CoDel() {
    std::lock_guard l{timer_lock};
    timer.cancel_all_events();
    timer.shutdown();
}

void CoDel::initialize(int64_t init_interval, int64_t init_target, bool coarse_interval, bool active){
    initial_interval = init_interval;
    initial_target_latency = init_target;
    adaptive_target = coarse_interval;
    if(active) {
        {
            std::lock_guard l{timer_lock};
            timer.cancel_all_events();
        }
        std::cout << "set timer" << std::endl;
        _interval_process(false);
    }
}

void CoDel::register_queue_latency(int64_t latency, int64_t size) {
    std::lock_guard l(register_lock);
    if (min_latency == INT_NULL || latency < min_latency) {
        min_latency = latency;
        min_latency_txc_size = size;
    }
    interval_size += size;
    txc_count++;
}

void CoDel::_interval_process(bool process) {
    std::lock_guard l(register_lock);
    if (process && min_latency != INT_NULL) {
        if (_check_latency_violation()) {
            // min latency violation
            violation_count++;
            violated_interval_count++;
            _update_interval();
            on_min_latency_violation(); // handle the violation
        } else {
            // no latency violation
            violation_count = 0;
            interval = initial_interval;
            on_no_violation();
        }
        min_lat_vec.push_back(min_latency);
        violation_count_vec.push_back(violation_count);
        no_violation_count_vec.push_back(violated_interval_count);

        // reset interval
        min_latency = INT_NULL;
        txc_count = 0;
        min_latency_txc_size = 0;
        interval_count++;
        interval_count_vec.push_back(interval_count);
        on_interval_finished();
        if (adaptive_target && interval_count >= slow_interval_frequency) {
            _coarse_interval_process();
        }
        target_lat_vec.push_back(target_latency);
        thr_vec.push_back(throughput);
    }

    auto codel_ctx = new LambdaContext(
            [this](int r) {
                _interval_process(true);
            });
    auto interval_duration = std::chrono::nanoseconds(interval);
    timer.add_event_after(interval_duration, codel_ctx);
}

void CoDel::_coarse_interval_process() {
    mono_clock::time_point now = mono_clock::now();
    auto time = std::chrono::nanoseconds(now - mono_clock::zero()).count();
    double_t interval_throughput = (interval_size * 1.0) / ((time - interval_time)/1000);
    if (interval_time > 0) {
        double violation_ratio = (violated_interval_count * 1.0) / slow_interval_frequency;
        if (violation_ratio < normal_codel_percentage_threshold) {
            target_latency -= target_increment;
        } else if (violation_ratio > aggressive_codel_percentage_threshold) {
            target_latency += target_increment;
        }
    }
    violated_interval_count = 0;
    interval_count = 0;
    interval_size = 0;
    interval_time = time;
    throughput = interval_throughput;
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
    interval = initial_interval / sqrt;
    if(interval <= 0){
        interval = 1000;
    }
}

/**
* reset the algorithm
*/
void CoDel::reset() {
    std::lock_guard l(register_lock);
    interval = initial_interval;
    target_latency = initial_target_latency;
    min_latency = INT_NULL;
    violated_interval_count = 0;
    interval_count = 0;
    interval_size = 0;
    interval_time = 0;
    std::cout << "target init:" << initial_target_latency << std::endl;
    std::cout << "target:" << target_latency << std::endl;
    std::cout << "interval init:" << initial_interval << std::endl;
    std::cout << "interval:" << interval << std::endl;
}
