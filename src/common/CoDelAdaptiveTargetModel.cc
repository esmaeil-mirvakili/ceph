
#include "CoDelAdaptiveTargetModel.h"

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
    adaptive_target = adaptive;
    activated = active;
    double tmp = 0;
    slope = &tmp;
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
    double time = 0;
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
        double time = std::chrono::nanoseconds(now - slow_interval_start).count();
        time = time / (1000 * 1000 * 1000.0);
        slow_interval_throughput = (coarse_interval_size * 1.0) / time;
        slow_interval_throughput /= 1024.0 * 1024.0;
        slow_interval_lat = (sum_latency / (1000 * 1000.0)) / slow_interval_txc_cnt;
        if (activated && adaptive_target) {
            switch (mode) {
                case NORMAL_PHASE:
                    double theta[2];
                    CoDelUtils::log_fit(slow_target_vec, slow_throughput_vec, theta);
                    target_latency = (theta[1] / beta) * 1000000;
                    cnt++;
                    if (cnt >= 10 * size_threshold) {
                        target_latency += range;
                        cnt = 0;
                        mode = CHECK_PHASE;
                        previous_target = target_latency;
                        target_latency = INT_NULL;
                        open_throttle();
                    }
                    break;
                case CONFIG_PHASE:
                    slow_target_vec.push_back(target_latency / 1000000.0);
                    slow_throughput_vec.push_back(slow_interval_throughput);
                    cnt++;
                    if (cnt >= size_threshold) {
                        target_latency += range;
                        cnt = 0;
                    }
                    if (target_latency > max_target_latency)
                        mode = NORMAL_PHASE;
                    break;
                case CHECK_PHASE:
                    open_throttle();
                    lat_sum += slow_interval_lat;
                    cnt++;
                    if (cnt >= 2 * size_threshold) {
                        double l = lat_sum / cnt;
                        if (previous_throughput == 0 || std::abs(l - previous_throughput) > 2) {
                            target_latency = min_target_latency;
                            mode = CONFIG_PHASE;
                        } else {
                            mode = NORMAL_PHASE;
                            target_latency = previous_target;
                        }
                        previous_throughput = l;
                        cnt = 0;
                        lat_sum = 0;
                        close_throttle();
                    }
                    break;
            }
        }
    }
    if(target_latency != INT_NULL) {
        target_latency = std::max(target_latency, min_target_latency);
        target_latency = std::min(target_latency, max_target_latency);
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
    if (target_latency != INT_NULL && min_latency != INT_NULL) {
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
    target_latency = initial_target_latency;
    min_latency = INT_NULL;
    coarse_interval_size = 0;
    slow_interval_start = mono_clock::zero();
    slow_interval_throughput = 0;
    slow_interval_lat = 0;
    cnt = 0;
    mode = CHECK_PHASE;
    throughput_max = 0;
    previous_target = 0;
    previous_throughput = 0;
    slow_throughput_vec.clear();
    slow_target_vec.clear();
    std::cout << "slow freq:" << slow_interval_frequency << std::endl;
    std::cout << "adaptive:" << adaptive_target << std::endl;
}
