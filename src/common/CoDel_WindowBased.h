
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

private:
    bool _check_latency_violation();
    void _update_interval();
    void _interval_process();

protected:
    int64_t initial_window_size;     // Initial interval to start the algorithm
    int64_t initial_target_latency;     // Initial target latency to start the algorithm
    int64_t window_size = INT_NULL;       // current interval that algorithm is using
    int64_t processed_size = 0;       // current interval that algorithm is using
    int64_t target_latency = INT_NULL;       // current target latency that algorithm is using
    int64_t min_latency = INT_NULL;       // min latency in the current interval
    int64_t violation_count = 0;       // number of consecutive violations

    /**
     * reset the algorithm
     */
    void reset();
    void register_queue_latency(int64_t queuing_latency, int64_t size);
    void initialize(int64_t init_window_size, int64_t init_target);

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

