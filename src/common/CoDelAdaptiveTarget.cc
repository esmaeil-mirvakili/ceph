
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
    txc_count++;
}

void CoDel::_interval_process(bool process) {
    std::lock_guard l(register_lock);
    if (process && min_latency != INT_NULL) {
        if (_check_latency_violation()) {
            // min latency violation
            violation_count++;
            _update_interval();
            on_min_latency_violation(); // handle the violation
        } else {
            // no latency violation
            violation_count = 0;
            no_violation_count++;
            interval = initial_interval;
            on_no_violation();
        }
        min_lat_vec.push_back(min_latency);
        violation_count_vec.push_back(violation_count);
        no_violation_count_vec.push_back(no_violation_count);
        int8_t coarse = 0;
        // reset interval
        min_latency = INT_NULL;
        txc_count = 0;
        min_latency_txc_size = 0;
        interval_count++;
        interval_count_vec.push_back(interval_count);
        on_interval_finished();
        if (adaptive_target && interval_count >= coarse_interval_frequency) {
            if(ignore_interval > 0)
                ignore_interval--;
            else {
                _coarse_interval_process();
                coarse = 1;
            }
        }
        coarse_vec.push_back(coarse);
        target_vec.push_back(target_latency);
    }

    auto codel_ctx = new LambdaContext(
            [this](int r) {
                _interval_process(true);
            });
    auto interval_duration = std::chrono::nanoseconds(interval);
    timer.add_event_after(interval_duration, codel_ctx);
}

void CoDel::_coarse_interval_process() {
    if((no_violation_count*1.0)/coarse_interval_frequency > 0.7){
//        if(has_bufferbloat_symptoms())
            target_latency -= target_increment;
    } else if((no_violation_count*1.0)/coarse_interval_frequency < 0.3){
//        if(!has_bufferbloat_symptoms())
            target_latency += target_increment;
    }
    no_violation_count = 0;
    interval_count = 0;
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
    no_violation_count = 0;
    interval_count = 0;
    ignore_interval = 10;
    std::cout << "target init:" << initial_target_latency << std::endl;
    std::cout << "target:" << target_latency << std::endl;
    std::cout << "interval init:" << initial_interval << std::endl;
    std::cout << "interval:" << interval << std::endl;
}
