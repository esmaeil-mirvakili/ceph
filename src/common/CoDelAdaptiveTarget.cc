
#include "CoDelAdaptiveTarget.h"

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
    double_t cur_throughput = -1;
    double_t avg_lat = -1;
    double_t time = 0;
    double sum = 0;
    delta = 1;
    auto target_temp = target_latency;
    bool ignore_interval = false;
    if (!mono_clock::is_zero(slow_interval_start) && slow_interval_txc_cnt > 0) {
        time = std::chrono::nanoseconds(now - slow_interval_start).count();
        time = time / (1000 * 1000 * 1000.0);
        cur_throughput = (coarse_interval_size * 1.0) / time;
        cur_throughput = cur_throughput / 1024;
        cur_throughput = cur_throughput / 1024;
        throughput_sliding_window.push_back(cur_throughput);
        if (throughput_sliding_window.size() > sliding_window_size)
            throughput_sliding_window.erase(throughput_sliding_window.begin());
        sum = 0;
        for (unsigned int i = 0; i < throughput_sliding_window.size(); i++)
            sum += throughput_sliding_window[i];
        cur_throughput = sum / throughput_sliding_window.size();

        avg_lat = (sum_latency / (1000 * 1000.0)) / slow_interval_txc_cnt;

        if (!optimize_using_target) {
            latency_sliding_window.push_back(avg_lat);
            if (latency_sliding_window.size() > sliding_window_size)
                latency_sliding_window.erase(latency_sliding_window.begin());
            sum = 0;
            for (unsigned int i = 0; i < latency_sliding_window.size(); i++)
                sum += latency_sliding_window[i];
            avg_lat = sum / latency_sliding_window.size();
        }

        if (optimize_using_target)
            delta_lat = (target_latency - slow_interval_target) / (1000 * 1000.0);
        else
            delta_lat = avg_lat - slow_interval_lat;

        delta_throughput = cur_throughput - slow_interval_throughput;
        if (activated && adaptive_target) {
            if (slow_interval_throughput >= 0 && slow_interval_lat >= 0) {
                if (std::abs(delta_lat) > lat_noise_threshold) {
                    if (delta_lat * delta_throughput < 0) {
                        ignore_interval = true;
                    } else {
                        delta = (delta_throughput - (beta * delta_lat)) / ((beta * delta_throughput) + delta_lat);
                        if (delta < 0)
                            delta = delta / beta;
                        else
                            delta = delta * beta;
                    }
                } else {
                    delta = delta_lat > 0 ? delta_threshold : -delta_threshold;
                }
            }
            if (delta == 0)
                ignore_interval = true;
            if (!ignore_interval)
                target_latency = target_latency + (delta * step_size);
        }
        target_latency = std::max(target_latency, min_target_latency);
        target_latency = std::min(target_latency, max_target_latency);
    }
    slow_interval_start = mono_clock::now();
    coarse_interval_size = 0;
    sum_latency = 0;
    slow_interval_txc_cnt = 0;
    if (target_latency != target_temp && !ignore_interval) {
        slow_interval_throughput = cur_throughput;
        slow_interval_lat = avg_lat;
        slow_interval_target = target_temp;
    }

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
    target_latency = initial_target_latency;
    min_latency = INT_NULL;
    coarse_interval_size = 0;
    slow_interval_start = mono_clock::zero();
    slow_interval_throughput = 0;
    slow_interval_lat = 0;
    std::cout << "slow freq:" << slow_interval_frequency << std::endl;
    std::cout << "adaptive:" << adaptive_target << std::endl;
}
