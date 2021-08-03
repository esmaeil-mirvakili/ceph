
#ifndef CEPH_CODEL_H
#define CEPH_CODEL_H


#include <cmath>
#include <iostream>
#include <algorithm>
#include <vector>
#include <map>
#include <fstream>
#include "ceph_time.h"
#include "common/Timer.h"
#include "include/Context.h"
#include "CoDelUtils.h"

#define INT_NULL -1
#define NORMAL_PHASE 0
#define CONFIG_PHASE 1
#define CHECK_PHASE 3

using ceph::mono_clock;
using TimePoint = CoDelUtils::DataPoint;

class CoDel {
public:
    CoDel(CephContext *_cct);
    ~CoDel();
    /**
    * reset the algorithm
    */
    void reset();
    bool activated = false;

private:
    bool _check_latency_violation();
    void _update_interval();
    void _interval_process();
    void _coarse_interval_process();

protected:
    int64_t initial_interval;     // Initial interval to start the algorithm
    int64_t initial_target_latency;     // Initial target latency to start the algorithm
    int64_t interval = INT_NULL;       // current interval that algorithm is using
    int64_t target_latency = INT_NULL;       // current target latency that algorithm is using
    int64_t min_latency = INT_NULL;       // min latency in the current interval
    int64_t min_target_latency = 1000000;  // in ns
    int64_t max_target_latency = 200000000; // in ns
    int64_t violation_count = 0;
    ceph::mutex fast_timer_lock = ceph::make_mutex("CoDel::fast_timer_lock");
    ceph::mutex slow_timer_lock = ceph::make_mutex("CoDel::slow_timer_lock");
    ceph::mutex register_lock = ceph::make_mutex("CoDel::register_lock");
    SafeTimer fast_timer;
    SafeTimer slow_timer;

    int64_t fast_interval_txc_cnt = 0;
    int64_t slow_interval_frequency = 10;
    mono_clock::time_point slow_interval_start = mono_clock::zero();
    double beta = 1;
    int64_t coarse_interval_size;
    int64_t sum_latency = 0;
    int64_t slow_interval_txc_cnt = 0;
    double slow_interval_throughput;
    double slow_interval_lat;

    std::vector<double> slow_target_vec;
    std::vector<double> slow_throughput_vec;
    bool adaptive_target = true;
    double *slope;
    double lat_sum = 0;
    double previous_throughput = 0;
    int64_t previous_target = 0;
    int64_t range;
    int64_t config_latency_threshold;
    int size_threshold;
    bool outlier_detection = false;
    int mode = CHECK_PHASE;
    int cnt = 0;

    void register_queue_latency(int64_t queuing_latency, double throttle_usage, int64_t size);
    void initialize(int64_t init_interval, int64_t init_target, bool adaptive_target, bool active);
    /**
     * react properly if min latency is greater than target latency (min latency violation)
     */
    virtual void on_min_latency_violation() = 0;
    /**
     * react properly if no latency violation detected
     */
    virtual void on_no_violation() = 0;
    virtual void on_interval_finished() = 0;
    virtual void open_throttle() = 0;
    virtual void close_throttle() = 0;
};


#endif //CEPH_CODEL_H

