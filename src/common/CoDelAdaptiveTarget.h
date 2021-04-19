
#ifndef CEPH_CODEL_H
#define CEPH_CODEL_H


#include <cmath>
#include <iostream>
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
    vector<int64_t> target_lat_vec;
    vector<int64_t> batch_vec;
    vector<int64_t> min_lat_vec;
    vector<int64_t> violation_count_vec;
    vector<int64_t> no_violation_count_vec;
    vector<int64_t> interval_count_vec;
    vector<double> time_vec;
    vector<double> thr_vec;

private:
    bool _check_latency_violation();
    void _update_interval();
    void _interval_process(bool process);
    void _coarse_interval_process();

protected:
    int64_t initial_interval;     // Initial interval to start the algorithm
    int64_t initial_target_latency;     // Initial target latency to start the algorithm
    int64_t interval = INT_NULL;       // current interval that algorithm is using
    int64_t target_latency = INT_NULL;       // current target latency that algorithm is using
    int64_t min_latency = INT_NULL;       // min latency in the current interval
    int64_t min_latency_txc_size = 0;
    int64_t violation_count = 0;       // number of consecutive violations
    int64_t no_violation_count = 0;       // number of non_violations
    int64_t interval_count = 0;       // number of passed intervals
    int64_t interval_size = 0;
    double interval_time = 0;
    double throughput = 0;
    int64_t txc_count = 0;
    int64_t coarse_interval_frequency = 20;
    int64_t target_increment = 100 * 1000;
    int64_t ignore_interval = 10;
    SafeTimer timer;
    ceph::mutex timer_lock = ceph::make_mutex("CoDel::timer_lock");
    ceph::mutex register_lock = ceph::make_mutex("CoDel::register_lock");
    bool adaptive_target = false;


    void register_queue_latency(int64_t queuing_latency, int64_t size);
    void initialize(int64_t init_interval, int64_t init_target, bool coarse_interval, bool active);

    /**
     * react properly if min latency is greater than target latency (min latency violation)
     */
    virtual void on_min_latency_violation() = 0;
    /**
     * react properly if no latency violation detected
     */
    virtual void on_no_violation() = 0;
    virtual void on_interval_finished() = 0;
    virtual bool has_bufferbloat_symptoms() = 0;
};


#endif //CEPH_CODEL_H

