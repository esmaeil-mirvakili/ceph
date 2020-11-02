
#ifndef CEPH_CODEL_H
#define CEPH_CODEL_H

#include <cmath>
#include <iostream>
#include "ceph_time.h"

#define INT_NULL -1

using ceph::mono_clock;

class CoDel {
public:
    CoDel(){}
    ~CoDel();
private:
    /**
     * check if the current interval is finished
     * @return true if current interval is finished, false otherwise
     */
    bool _is_cur_interval_finished() {
        auto current_time = mono_clock::now();
        return std::chrono::nanoseconds(current_time - *interval_start).count() >= interval;
    }

    bool _check_latency_violation() {
        return min_latency > target_latency;
    }

    void _update_interval() {
        auto sqrt = (int) std::round(std::sqrt(violation_count));
        interval = initial_interval / sqrt;
        if(interval <= 0){
            interval = 1000;
        }
    }

protected:
    int64_t initial_interval;     // Initial interval to start the algorithm
    int64_t initial_target_latency;     // Initial target latency to start the algorithm
    int64_t interval = INT_NULL;       // current interval that algorithm is using
    int64_t target_latency = INT_NULL;       // current target latency that algorithm is using
    int64_t min_latency = INT_NULL;       // min latency in the current interval
    mono_clock::time_point *interval_start = nullptr;      // beginning of current interval
    int64_t violation_count = 0;       // number of consecutive violations

    /**
     * react properly if min latency is greater than target latency (min latency violation)
     */
    virtual void on_min_latency_violation() = 0;

    /**
     * react properly if no latency violation detected
     */
    virtual void on_no_violation() = 0;

    virtual void on_interval_finished() = 0;

    /**
     * reset the algorithm
     */
    void reset() {
        interval = initial_interval;
        target_latency = initial_target_latency;
        std::cout << "target init:" << initial_target_latency << std::endl;
        std::cout << "target:" << target_latency << std::endl;
        std::cout << "interval init:" << initial_interval << std::endl;
        std::cout << "interval:" << interval << std::endl;
        interval_start = nullptr;
        min_latency = INT_NULL;
    }

    /**
     * reset the interval
     */
    void reset_interval() {
        min_latency = INT_NULL;
        auto now = mono_clock::now();
        interval_start = &now;
        on_interval_finished();
    }

    void register_queue_latency(mono_clock::duration queuing_latency);
};


#endif //CEPH_CODEL_H
