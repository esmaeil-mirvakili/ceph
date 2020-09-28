
#include "CoDel.h"

void CoDel::register_queue_latency(mono_clock::duration queuing_latency) {
    std::cout << "1" << std::endl;
    if(!min_latency || queuing_latency < *min_latency){
        std::cout << "2" << std::endl;
        min_latency = &queuing_latency;
        std::cout << "3" << std::endl;
    }
    if(!interval_start){
        std::cout << "5" << std::endl;
        auto now = mono_clock::now();
        interval_start = &now;
        std::cout << "6" << std::endl;
    } else if (_is_cur_interval_finished()){
        std::cout << "7" << std::endl;
        if(_check_latency_violation()){
            // min latency violation
            std::cout << "8" << std::endl;
            violation_count++;
            _update_interval();
            std::cout << "9" << std::endl;
            on_min_latency_violation(); // handle the violation
            std::cout << "10" << std::endl;
        } else{
            // no latency violation
            std::cout << "11" << std::endl;
            violation_count = 0;
            auto d = mono_clock::duration(initial_interval->count());
            interval = &d;
            std::cout << "12" << std::endl;
            on_no_violation();
            std::cout << "13" << std::endl;
        }
        // reset interval
        std::cout << "14" << std::endl;
        reset_interval();
        std::cout << "15" << std::endl;
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