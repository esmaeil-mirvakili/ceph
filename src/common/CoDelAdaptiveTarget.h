
#ifndef CEPH_CODEL_H
#define CEPH_CODEL_H


#include <cmath>
#include <iostream>
#include <vector>
#include <map>
#include "ceph_time.h"
#include "common/Timer.h"
#include "include/Context.h"

#define INT_NULL -1

using ceph::mono_clock;

class CoDel {
public:
    CoDel(CephContext *_cct);
    ~CoDel();
    /**
    * reset the algorithm
    */
    void reset();

private:
    bool _check_latency_violation();
    void _update_interval();
    void _interval_process();
    void _coarse_interval_process();

protected:
    bool activated = false;
    int64_t initial_interval;     // Initial interval to start the algorithm
    int64_t initial_target_latency;     // Initial target latency to start the algorithm
    int64_t interval = INT_NULL;       // current interval that algorithm is using
    int64_t target_latency = INT_NULL;       // current target latency that algorithm is using
    int64_t min_latency = INT_NULL;       // min latency in the current interval
    int64_t sum_latency = 0;
    int64_t min_target_latency = 1000000;  // in ns
    int64_t max_target_latency = 200000000; // in ns
    int64_t txc_cnt = 0;
    int64_t violation_count = 0;
    int64_t slow_interval_frequency = 10;
    mono_clock::time_point slow_interval_start = mono_clock::zero();
    double_t slow_interval_throughput;
    double_t slow_interval_lat;
    double_t bw_noise_threshold;
    double_t lat_noise_threshold;
    double_t slow_interval_target;
    double_t step_size = 0.01;
    double_t beta = 1;
    double_t lat_normalization_factor = 1;
    vector<double_t> sliding_window;
    int sliding_window_size = 10;
    int64_t interval_count = 0;
    int64_t coarse_interval_size;
    SafeTimer fast_timer;
    SafeTimer slow_timer;
    ceph::mutex fast_timer_lock = ceph::make_mutex("CoDel::fast_timer_lock");
    ceph::mutex slow_timer_lock = ceph::make_mutex("CoDel::slow_timer_lock");
    ceph::mutex register_lock = ceph::make_mutex("CoDel::register_lock");
    bool adaptive_target = false;
    double_t delta;


    void register_queue_latency(int64_t queuing_latency, double_t throttle_usage, int64_t size);
    void initialize(int64_t init_interval, int64_t init_target, bool adaptive_target, bool active, double beta_deg);

    /**
     * react properly if min latency is greater than target latency (min latency violation)
     */
    virtual void on_min_latency_violation() = 0;
    /**
     * react properly if no latency violation detected
     */
    virtual void on_no_violation() = 0;
    virtual void on_interval_finished() = 0;
};


#endif //CEPH_CODEL_H

