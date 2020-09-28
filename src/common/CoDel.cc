
#include "CoDel.h"

void CoDel::register_queue_latency(mono_clock::duration queuing_latency) {
    if(!min_latency || queuing_latency.count() < min_latency->count()){
        min_latency = new mono_clock::duration(queuing_latency.count());
    }
    if(!interval_start){
        auto now = mono_clock::now();
        interval_start = &now;
    } else if (_is_cur_interval_finished()){
        if(_check_latency_violation()){
            // min latency violation
            violation_count++;
            _update_interval();
            on_min_latency_violation(); // handle the violation
        } else{
            // no latency violation
            violation_count = 0;
            auto d = mono_clock::duration(initial_interval->count());
            interval = &d;
            on_no_violation();
        }
        // reset interval
        reset_interval();
    }
}

CoDel::~CoDel() {
    // delete pointers
//    delete initial_interval;
//    delete initial_target_latency;
//    delete interval;
//    delete target_latency;
//    delete min_latency;
//    delete interval_start;
}