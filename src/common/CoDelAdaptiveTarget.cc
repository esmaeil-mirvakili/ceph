
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

    auto codel_ctx = new LambdaContext(
            [this](int r) {
                _interval_process();
            });
    auto interval_duration = std::chrono::nanoseconds(interval);
    fast_timer.add_event_after(interval_duration, codel_ctx);
}

double_t CoDel::_estimate_slope_by_regression(vector<TimePoint> time_points) {
    double_t X_mean = 0;
    double_t Y_mean = 0;
    double_t SS_xy = 0;
    double_t SS_xx = 0;
    double_t multiply_sum = 0;
    double_t time_square_sum = 0;
    int n = time_points.size();
    for (unsigned int i = 0; i < time_points.size(); i++) {
        X_mean += time_points[i].time;
        Y_mean += time_points[i].value;
        multiply_sum += (time_points[i].time * time_points[i].value);
        time_square_sum += (time_points[i].time * time_points[i].time);
    }
    X_mean /= n;
    Y_mean /= n;
    SS_xy = multiply_sum - (n * X_mean * Y_mean);
    SS_xx = time_square_sum - (n * X_mean * X_mean);
    return SS_xy / SS_xx;
}

vector<TimePoint> CoDel::_moving_average(vector<TimePoint> time_points, int window_size) {
    vector<TimePoint> temp;
    for (unsigned int i = 0; i < (time_points.size() - window_size); i++) {
        double_t sum = 0;
        for (unsigned int j = i; j < i + window_size; j++)
            sum += time_points[j].value;
        TimePoint time_point = {time_points[i].time, sum / window_size};
        temp.push_back(time_point);
    }
    return temp;
}

void CoDel::_add_time_point(double_t time, double_t value) {
    if(time > time_limit)
        time_limit = time;
    if(time < time_limit - time_window_duration)
        time_limit = time + time_window_duration - 1;

    TimePoint time_point = {time, value};
    time_series.push_back(time_point);

    if (time_series.size() > time_window_size && time_window_size > 0)
        time_series.erase(time_series.begin());

    vector<TimePoint> temp;
    for (unsigned int i = 0; i < time_series.size(); i++) {
        if (time_series[i].time <= time_limit && time_series[i].time >= time_limit - time_window_duration)
            temp.push_back(time_series[i]);
    }
    time_series = temp;
}

void CoDel::_coarse_interval_process() {
    std::lock_guard l(register_lock);
    mono_clock::time_point now = mono_clock::now();

    if (!mono_clock::is_zero(slow_interval_start) && slow_interval_txc_cnt > 0) {
        double_t time = std::chrono::nanoseconds(now - slow_interval_start).count();
        time = time / (1000 * 1000 * 1000.0);
        slow_interval_throughput = (coarse_interval_size * 1.0) / time;
        slow_interval_throughput /= 1024.0 * 1024.0;
        slow_interval_lat = (sum_latency / (1000 * 1000.0)) / slow_interval_txc_cnt;
        auto temp_target = target_latency;
        temp_target /= 1000000;
        if (activated && adaptive_target) {
            {
                vector<TimePoint> temp = time_series;
                std::sort(temp.begin(), temp.end(), CoDel::compare_time_point);
                if(smoothing_activated)
                    temp = _moving_average(temp, smoothing_window);
                slope = _estimate_slope_by_regression(temp);
            }
            if(slope >= 0){
                delta = (slope - beta) / (beta * slope + 1);
                if(delta < 0)
                    delta /= beta;
                else
                    delta *= beta;
            } else{
                delta = -1;
            }
            if( delta > 0)
                delta = std::max(delta, delta_threshold);
            else
                delta = std::min(delta, - delta_threshold);
            target_latency += delta * step_size;
            _add_time_point(temp_target, slow_interval_throughput);
        }
    } else{
        target_latency += step_size;
    }
    target_latency = std::max(target_latency, min_target_latency);
    target_latency = std::min(target_latency, max_target_latency);

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
    target_latency = initial_target_latency;
    min_latency = INT_NULL;
    coarse_interval_size = 0;
    slow_interval_start = mono_clock::zero();
    slow_interval_throughput = 0;
    slow_interval_lat = 0;
    std::cout << "slow freq:" << slow_interval_frequency << std::endl;
    std::cout << "adaptive:" << adaptive_target << std::endl;
}
