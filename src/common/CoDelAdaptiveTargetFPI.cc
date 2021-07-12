
#include "CoDelAdaptiveTargetFPI.h"

CoDel::CoDel(CephContext *_cct) : fast_timer(_cct, fast_timer_lock), slow_timer(_cct, slow_timer_lock) {
    fast_timer.init();
    slow_timer.init();
}

CoDel::~CoDel() {
    std::lock_guard l1{fast_timer_lock};
    fast_timer.cancel_all_events();
    fast_timer.shutdown();

    std::lock_guard l2{slow_timer_lock};
    slow_timer.cancel_all_events();
    slow_timer.shutdown();
}

void CoDel::initialize(int64_t init_interval, int64_t init_target, bool adaptive, bool active) {
    initial_interval = init_interval;
    initial_target_latency = init_target;
    target_latency = min_target_latency;
    reconfigure_phase = true;
    adaptive_target = adaptive;
    activated = active;
    {
        std::lock_guard l1{fast_timer_lock};
        fast_timer.cancel_all_events();
    }
    _interval_process();
    {
        std::lock_guard l2{slow_timer_lock};
        slow_timer.cancel_all_events();
    }
    _coarse_interval_process();
}

void CoDel::register_queue_latency(int64_t latency, double_t throttle_usage, int64_t size) {
    std::lock_guard l(register_lock);
    if (min_latency == INT_NULL || latency < min_latency) {
        min_latency = latency;
    }
    sum_latency += latency;
    slow_interval_txc_cnt++;
    coarse_interval_size += size;
}

void CoDel::_interval_process() {
    std::lock_guard l(register_lock);
    double_t time = 0;
    mono_clock::time_point now = mono_clock::now();
    if (min_latency != INT_NULL) {
        if (activated) {
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
        }

        // reset interval
        min_latency = INT_NULL;
        on_interval_finished();
    }
    fast_interval_start = mono_clock::now();
    auto codel_ctx = new LambdaContext(
            [this](int r) {
                _interval_process();
            });
    auto interval_duration = std::chrono::nanoseconds(interval);
    fast_timer.add_event_after(interval_duration, codel_ctx);
}

void CoDel::_coarse_interval_process() {
    std::lock_guard l(register_lock);
    mono_clock::time_point now = mono_clock::now();

    if (!mono_clock::is_zero(slow_interval_start) && slow_interval_txc_cnt > 0) {
        double_t time = std::chrono::nanoseconds(now - slow_interval_start).count();
        time = time / (1000 * 1000 * 1000.0);
        double_t cur_throughput = (coarse_interval_size * 1.0) / time;
        cur_throughput = cur_throughput / 1024;
        cur_throughput = cur_throughput / 1024;
        throughput_cnt++;
        throughput_sum += cur_throughput;
        if (reconfigure_phase){
            target_latency = min_target_latency;
            if (throughput_cnt >= sliding_window_size){
                reconfigure_phase = false;
                base_throughput = throughput_sum / (throughput_cnt * 1.0);
                throughput_sum = 0;
                throughput_cnt = 0;
                target_latency += step_size;
            }
        } else{
//            auto normalized_target = (target_latency - min_target_latency);
            if (throughput_cnt >= sliding_window_size) {
                auto th = throughput_sum / (throughput_cnt * 1.0);
                auto normalized_throughput = cur_throughput - base_throughput;
                delta_throughput = normalized_throughput;
                if (normalized_throughput > 0) {
                    auto new_normalized_target = normalized_throughput / beta;
                    target_latency = min_target_latency + new_normalized_target;
                    delta_lat = target_latency;
                    throughput_sum = 0;
                    throughput_cnt = 0;
                }
            }
        }
        target_latency = std::max(target_latency, min_target_latency);
        target_latency = std::min(target_latency, max_target_latency);

        slow_interval_throughput = cur_throughput;
        slow_interval_lat = (sum_latency / (1000 * 1000.0)) / slow_interval_txc_cnt;
    }
    slow_interval_start = mono_clock::now();
    coarse_interval_size = 0;
    sum_latency = 0;
    slow_interval_txc_cnt = 0;

    auto codel_ctx = new LambdaContext(
            [this](int r) {
                _coarse_interval_process();
            });
    auto interval_duration = std::chrono::nanoseconds(initial_interval * slow_interval_frequency);
    slow_timer.add_event_after(interval_duration, codel_ctx);
}

/**
* check if the min latency violate the target
* @return true if min latency violate the target, false otherwise
*/
bool CoDel::_check_latency_violation() {
    if (min_latency != INT_NULL) {
        if (min_latency > target_latency)
            return true;
    }
    return false;
}

void CoDel::_update_interval() {
    auto sqrt = (int) std::round(std::sqrt(violation_count));
    interval = initial_interval / sqrt;
    if (interval <= 0) {
        interval = 1000;
    }
}

/**
* reset the algorithm
*/
void CoDel::reset() {
    std::lock_guard l(register_lock);
    interval = initial_interval;
    target_latency = min_target_latency;
    min_latency = INT_NULL;
    coarse_interval_size = 0;
    slow_interval_start = mono_clock::zero();
    reconfigure_phase = true;
    throughput_sum = 0;
    throughput_cnt = 0;
    std::cout << "slow freq:" << slow_interval_frequency << std::endl;
    std::cout << "adaptive:" << adaptive_target << std::endl;
}
