
#include "CoDelAdaptiveTargetModel.h"

CoDel::CoDel(CephContext *_cct) : fast_timer(_cct, fast_timer_lock), slow_timer(_cct, slow_timer_lock){//},
                                  //logfile("/users/esmaeil/log.txt") {
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
    if (target_latency != INT_NULL && min_latency != INT_NULL) {
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
//    logfile << "1" << std::endl;
//    logfile.flush();
    if (!mono_clock::is_zero(slow_interval_start) && slow_interval_txc_cnt > 0) {
//        logfile << "1.1" << std::endl;
//        logfile.flush();
        double time = std::chrono::nanoseconds(now - slow_interval_start).count();
        time = time / (1000 * 1000 * 1000.0);
        slow_interval_throughput = (coarse_interval_size * 1.0) / time;
        slow_interval_throughput /= 1024.0 * 1024.0;
        slow_interval_lat = (sum_latency / (1000 * 1000.0)) / slow_interval_txc_cnt;
//        logfile << "1.2" << std::endl;
//        logfile.flush();
        if (activated && adaptive_target) {
//            logfile << "1.3" << std::endl;
//            logfile.flush();
//            int index = static_cast<int>(std::floor((target_latency - min_target_latency) / config_latency_threshold));
//            if (index >= range_cnt)
//                index = range_cnt - 1;
//            logfile << "1.3.1: index = " << index << std::endl;
//            logfile << "1.3.1: target_latency = " << target_latency << std::endl;
//            logfile << "1.3.1: range = " << config_latency_threshold << std::endl;
//            logfile.flush();
            slow_target_vec.push_back(target_latency / 1000000.0);
//            logfile << "1.3.2" << std::endl;
//            logfile.flush();
            slow_throughput_vec.push_back(slow_interval_throughput);
//            logfile << "1.4" << std::endl;
//            logfile.flush();
            switch (mode) {
                case CONFIG_PHASE:
                case NORMAL_PHASE: {
//                    logfile << "1.5" << std::endl;
//                    logfile.flush();
                    if (slow_target_vec.size() > size_threshold) {
                        slow_target_vec.erase(slow_target_vec.begin());
                        slow_throughput_vec.erase(slow_throughput_vec.begin());
                    }
//                    logfile << "1.6" << std::endl;
//                    logfile.flush();
                    std::vector<double> targets;
                    std::vector<double> throughputs;
//                    logfile << "1.7" << std::endl;
//                    logfile.flush();
                    double theta[2];
                    CoDelUtils::log_fit(slow_target_vec, slow_throughput_vec, theta, outlier_detection);
//                    logfile << "1.8" << std::endl;
//                    logfile.flush();
                    double target = (theta[1] / beta);
//                    logfile << "1.9" << std::endl;
//                    logfile.flush();
                    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
                    std::default_random_engine generator(seed);
                    double dist_params[2];
                    CoDelUtils::find_log_normal_dist_params(target, min_target_latency / 1000000,
                                                            max_target_latency / 1000000, dist_params);
//                    logfile << "1.10" << std::endl;
//                    logfile.flush();
                    std::lognormal_distribution<double> distribution(dist_params[0], dist_params[1]);
                    target_latency = distribution(generator) * 1000000.0 + min_target_latency;
                }
                    break;
//                case CONFIG_PHASE:
////                    logfile << "1.11" << std::endl;
////                    logfile.flush();
//                    cnt++;
//                    if (cnt >= size_threshold) {
//                        target_latency += range;
//                        cnt = 0;
//                    }
//                    if (target_latency > max_target_latency) {
//                        mode = NORMAL_PHASE;
////                        model_size = slow_target_vec.size();
//                    }
//                    break;
            }
        }
    }
//    logfile << "2" << std::endl;
//    logfile.flush();
    if (target_latency != INT_NULL) {
        target_latency = std::max(target_latency, min_target_latency);
        target_latency = std::min(target_latency, max_target_latency);
    }
//    logfile << "3" << std::endl;
//    logfile.flush();
    slow_interval_start = mono_clock::now();
    coarse_interval_size = 0;
    sum_latency = 0;
    slow_interval_txc_cnt = 0;
//    logfile << "4" << std::endl;
//    logfile.flush();
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
    mode = NORMAL_PHASE;
    lat_sum = 0;
    previous_target = 0;
    previous_throughput = 0;
    slow_throughput_vec.clear();
    slow_target_vec.clear();
//    range_cnt = static_cast<int>(std::floor((max_target_latency - min_target_latency) / (config_latency_threshold * 1.0))) + 1;
//    logfile << "range cnt: " << range_cnt << std::endl;
//    logfile << "max_target_latency: " << max_target_latency << std::endl;
//    logfile << "min_target_latency: " << min_target_latency << std::endl;
//    logfile.flush();
//    for (int i = 0; i < range_cnt; i++) {
//        std::vector<double> th_vec;
//        std::vector<double> target_vec;
//        slow_throughput_vec.push_back(th_vec);
//        slow_target_vec.push_back(target_vec);
//    }
//    logfile << "target vec: " << slow_target_vec.size() << std::endl;
//    logfile.flush();
//    logfile << "th vec: " << slow_throughput_vec.size() << std::endl;
//    logfile.flush();
}
