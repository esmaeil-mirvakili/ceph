
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
    adaptive_target = adaptive;
    activated = active;
    fx_size = (max_target_latency - min_target_latency) / step_size;
    throughput_target_fx = new ThroughputInfo[fx_size];
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

int64_t CoDel::get_index() {
    int index = selected_target_index + selected_target_offset;
    return index;
}

int64_t CoDel::calculate_target() {
    return min_target_latency + (get_index() * step_size);
}

double_t CoDel::calculate_throughput(ThroughputInfo throughputInfo) {
    double_t sum = 0;
    for (unsigned int i = 0; i < throughputInfo.throughput_vec.size(); i++)
        sum += throughputInfo.throughput_vec[i];
    return sum / (throughputInfo.throughput_vec.size() * );
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

void CoDel::re_configure(int offset) {
    reconfigure = true;
    selected_target_offset = offset;
    if (get_index() >= 0 && get_index() <= fx_size - 1)
        throughput_target_fx[get_index()].throughput_vec.clear();
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

    double_t time = std::chrono::nanoseconds(now - slow_interval_start).count();
    time = time / (1000 * 1000 * 1000.0);

    double_t cur_throughput = (coarse_interval_size * 1.0) / time;
    cur_throughput = cur_throughput / 1024;
    cur_throughput = cur_throughput / 1024;

//    double_t avg_lat = (sum_latency / (1000 * 1000.0)) / slow_interval_txc_cnt;
    if (get_index() < 0)
        re_configure(1);
    if (get_index() > fx_size - 1)
        selected_target_offset = 0;
    throughput_target_fx[get_index()].throughput_vec.push_back(cur_throughput);
    if (throughput_target_fx[get_index()].throughput_vec.size() > sliding_window_size) {
        auto begin = throughput_target_fx[get_index()].throughput_vec.begin();
        throughput_target_fx[get_index()].throughput_vec.erase(begin);

        if (reconfigure) {
            if (selected_target_offset < 0)
                re_configure(1);
            else
                selected_target_offset = 0;
        } else {
            double_t throughput = calculate_throughput(throughput_target_fx[get_index()]);
            double_t prev_throughput = 0;
            double_t prev_slope = 0;
            double_t next_throughput = 0;
            double_t next_slope = 0;
            double_t step_ms = step_size / (1000 * 1000.0);

            if (get_index() == 0)
                prev_slope = 1;
            else {
                if (throughput_target_fx[get_index() - 1].throughput_vec.size() == 0)
                    re_configure(-1);
                else {
                    prev_throughput = calculate_throughput(throughput_target_fx[get_index() - 1]);
                    prev_slope = (throughput - prev_throughput) / step_ms;
                }
            }

            if (get_index() == fx_size - 1)
                next_slope = 0;
            else {
                if (throughput_target_fx[get_index() + 1].throughput_vec.size() == 0)
                    re_configure(1);
                else {
                    next_throughput = calculate_throughput(throughput_target_fx[get_index() + 1]);
                    next_slope = (next_throughput - throughput) / step_ms;
                }
            }
            if (!reconfigure)
                if (prev_slope > 0 && next_slope >= 0) {
                    if (prev_slope > next_slope) {
                        if (next_slope > beta && selected_target_index < fx_size - 1) {
                            if (get_index() > 0)
                                throughput_target_fx[get_index() - 1].throughput_vec.clear();
                            selected_target_index++;
                        } else if (prev_slope < beta && selected_target_index > 0) {
                            if (get_index() < fx_size - 1)
                                throughput_target_fx[get_index() + 1].throughput_vec.clear();
                            selected_target_index--;
                        }
                    } else {
                        re_configure(-1)
                    }
                } else {
                    re_configure(-1)
                }
        }

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
        if (min_latency > calculate_target())
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
    min_latency = INT_NULL;
    coarse_interval_size = 0;
    slow_interval_start = mono_clock::zero();
    slow_interval_throughput = 0;
    slow_interval_lat = 0;
    std::cout << "slow freq:" << slow_interval_frequency << std::endl;
    std::cout << "adaptive:" << adaptive_target << std::endl;
}
