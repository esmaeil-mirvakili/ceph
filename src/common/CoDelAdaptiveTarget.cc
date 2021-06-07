
#include "CoDelAdaptiveTarget.h"

CoDel::CoDel(CephContext *_cct): fast_timer(_cct, fast_timer_lock), slow_timer(_cct, slow_timer_lock){
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

void CoDel::initialize(int64_t init_interval, int64_t init_target, bool adaptive, bool active){
    initial_interval = init_interval;
    initial_target_latency = init_target;
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
    txc_cnt++;
    coarse_interval_size += size;
}

void CoDel::_interval_process() {
    std::lock_guard l(register_lock);
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
    double_t cur_throughput = 0;
    double_t avg_lat = 0;
    double_t time = 0;
    delta = 0;
    int64_t max_step = 500000;
    auto target_temp = target_latency;
    if (!mono_clock::is_zero(slow_interval_start) && txc_cnt > 0) {
        time = std::chrono::nanoseconds(now - slow_interval_start).count();
        time = time / 1000000000.0;
        cur_throughput = (coarse_interval_size * 1.0) / time;
        cur_throughput = cur_throughput / 1024;
        cur_throughput = cur_throughput / 1024;
        avg_lat = (sum_latency / 1000000.0) / txc_cnt;
        auto delta_lat = avg_lat - slow_interval_lat;
        auto delta_throughput = cur_throughput - slow_interval_throughput;
        if (activated && adaptive_target) {
            if (slow_interval_throughput > 0 && slow_interval_target > 0) {
                if(delta_lat * delta_throughput < 0){
                    delta = - max_step;
                }else{
                    delta = (delta_throughput - delta_lat)/(delta_throughput + delta_lat);
                    if(delta_lat < 0){
                        delta = - delta;
                    }
                }
            }
            target_latency = target_latency + delta;
        }
    }
    slow_interval_start = mono_clock::now();
    coarse_interval_size = 0;
    sum_latency = 0;
    txc_cnt = 0;
    slow_interval_throughput = cur_throughput;
    slow_interval_lat = avg_lat;
    slow_interval_target = target_temp;

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
    coarse_interval_size = 0;
    slow_interval_start = mono_clock::zero();
    coarse_interval_size = 0;
    slow_interval_throughput = 0;
    slow_interval_lat = 0;
    std::cout << "slow freq:" << slow_interval_frequency << std::endl;
    std::cout << "adaptive:" << adaptive_target << std::endl;
}
