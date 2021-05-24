
#include "CoDelAdaptiveTarget.h"

CoDel::CoDel(CephContext *_cct): timer(_cct, timer_lock){
    timer.init();
}

CoDel::~CoDel() {
    std::lock_guard l{timer_lock};
    timer.cancel_all_events();
    timer.shutdown();
}

void CoDel::initialize(int64_t init_interval, int64_t init_target, bool adaptive, bool active){
    initial_interval = init_interval;
    initial_target_latency = init_target;
    adaptive_target = adaptive;
    if(active) {
        {
            std::lock_guard l{timer_lock};
            timer.cancel_all_events();
        }
        _interval_process();
    }
}

void CoDel::register_queue_latency(int64_t latency, double_t throttle_usage, int64_t size) {
    std::lock_guard l(register_lock);
    if (min_latency == INT_NULL || latency < min_latency) {
        min_latency = latency;
    }
    sum_latency += latency;
    txc_cnt++;
    coarse_interval_size += size;
}

void CoDel::_interval_process() {
    std::lock_guard l(register_lock);
    if (min_latency != INT_NULL) {
        if (_check_latency_violation()) {
            // min latency violation
            violation_count++;
            _update_interval();
            on_min_latency_violation(); // handle the violation
        } else {
            // no latency violation
            violation_count = 0;
            interval = initial_interval;
            on_no_violation();
        }

        // reset interval
        min_latency = INT_NULL;
        interval_count++;
        if (adaptive_target && interval_count >= slow_interval_frequency) {
            _coarse_interval_process();
        }
        on_interval_finished();
    }

    auto codel_ctx = new LambdaContext(
            [this](int r) {
                _interval_process();
            });
    auto interval_duration = std::chrono::nanoseconds(interval);
    timer.add_event_after(interval_duration, codel_ctx);
}

void CoDel::_coarse_interval_process() {
    mono_clock::time_point now = mono_clock::now();
    double_t cur_throughput = 0;
    double_t avg_lat = 0;
    int64_t time = 0;
    if (!mono_clock::is_zero(slow_interval_start)) {
        time = std::chrono::nanoseconds(now - slow_interval_start).count();
//        time = time / 1000000.0;    // to ms
        cur_throughput = (coarse_interval_size * 1.0) / time;
        avg_lat = (sum_latency * 1.0) / txc_cnt;
        auto cur_loss = pow(avg_lat, 0.5) / cur_throughput;
        auto pre_loss = pow(slow_interval_lat, 0.5) / slow_interval_throughput;
        if (slow_interval_throughput > 0){
            delta = -(learning_rate * (cur_loss - pre_loss))/(target_latency - slow_interval_target);
            slow_interval_target = target_latency;
            if (target_latency + delta >= min_target_latency && target_latency + delta <= max_target_latency)
                target_latency = target_latency + delta;
        }
    }
    interval_count = 0;
    slow_interval_start = mono_clock::now();
    coarse_interval_size = 0;
    sum_latency = 0;
    txc_cnt = 0;
    slow_interval_throughput = cur_throughput;
    slow_interval_lat = avg_lat;
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
    interval_count = 0;
    coarse_interval_size = 0;
    slow_interval_start = mono_clock::zero();
    coarse_interval_size = 0;
    slow_interval_throughput = 0;
    slow_interval_lat = 0;
    std::cout << "slow freq:" << slow_interval_frequency << std::endl;
    std::cout << "adaptive:" << adaptive_target << std::endl;
}
