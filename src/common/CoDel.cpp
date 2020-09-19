
#include "CoDel.h"

void CoDel::register_queue_latency(mono_clock::duration queuing_latency) {
    if(!min_latency || queuing_latency < min_latency){
        min_latency = queuing_latency;
    }
    if(!interval_start){
        interval_start = mono_clock::now();
    } else if (_is_cur_interval_finished()){
        if(_check_latency_violation()){
            // min latency violation
            violation_count++;
            _update_interval();

            on_min_latency_violation(); // handle the violation

        } else{
            // no latency violation
            violation_count = 0;
            interval = initial_interval;

            on_no_violation();
        }
        // reset interval
        reset_interval();
    }
}