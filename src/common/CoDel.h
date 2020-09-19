
#ifndef CEPH_CODEL_H
#define CEPH_CODEL_H

#include <cmath>
#include "ceph_time.h"

using ceph::mono_clock;

class CoDel {
private:
    /**
     * check if the current interval is finished
     * @return true if current interval is finished, false otherwise
     */
    bool _is_cur_interval_finished() {
        auto current_time = mono_clock::now();
        return current_time - interval_start >= interval;
    }

    bool _check_latency_violation() {
        return min_latency > target_latency;
    }

    void _update_interval() {
        auto sqrt = (int) std::round(std::sqrt(violation_count));
        mono_clock::duration d(sqrt);
        interval = initial_interval / d;
        if(interval.count() <= 0){
            interval = mono_clock::duration(1);
        }
    }

    void register_queue_latency(mono_clock::duration queuing_latency);

protected:
    mono_clock::duration initial_interval(1000);     // Initial interval to start the algorithm
    mono_clock::duration initial_target_latency(500);     // Initial target latency to start the algorithm
    mono_clock::duration interval;       // current interval that algorithm is using
    mono_clock::duration target_latency;       // current target latency that algorithm is using
    mono_clock::duration min_latency = NULL;       // min latency in the current interval
    mono_clock::time_point interval_start = NULL;      // beginning of current interval
    int64_t violation_count = 0;       // number of consecutive violations

    /**
     * react properly if min latency is greater than target latency (min latency violation)
     */
    virtual void on_min_latency_violation();

    /**
     * react properly if no latency violation detected
     */
    virtual void on_no_violation();

    /**
     * reset the algorithm
     */
    void reset() {
        interval = initial_interval;
        target_latency = initial_target_latency;
        interval_start = NULL;
        min_latency = NULL;
    }

    /**
     * reset the interval
     */
    void reset_interval() {
        min_latency = NULL;
        interval_start = mono_clock::now();
    }
};


#endif //CEPH_CODEL_H
