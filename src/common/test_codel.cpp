
#include <cstdlib>
#include <ctime>
#include <thread>
#include <cmath>
#include <gtest/gtest.h>
#include "CoDel.h"

class TestCoDel: public CoDel{
public:
    int test_violation_count = 0;
    int test_no_violation_count = 0;

    mono_clock::duration get_interval(){
        return interval;
    }

    int get_violation_count(){
        return violation_count;
    }

protected:
    void on_min_latency_violation(){
        test_violation_count++;
    }

    void on_no_violation(){
        test_no_violation_count++;
    }
};

TestCoDel generate_test_codel(int target_latency, int interval){
    auto init_target = target_latency;
    mono_clock::duration init_interval(interval);
    mono_clock::duration init_target_latency(init_target);
    TestCoDel coDel(init_interval, init_target_latency);
}

TEST(CoDelTest, noViolationTest){
    srand((unsigned) time(0));

    TestCoDel coDel = generate_test_codel(100, 50);

    auto op_num = 100;
    for(auto i = 0; i < op_num; i++){
        mono_clock::duration latency(init_target - (rand() % 50 - 1)); // always less than target latency
        coDel.register_queue_latency(latency);
        mono_clock::duration timeout(10);
        std::this_thread::sleep_for(timeout);
    }
    ASSERT_TRUE(coDel.test_no_violation_count);
    ASSERT_FALSE(coDel.test_violation_count);
}

TEST(CoDelTest, noViolationTest){
    srand((unsigned) time(0));

    TestCoDel coDel = generate_test_codel(100, 50);

    auto op_num = 100;
    for(auto i = 0; i < op_num; i++){
        mono_clock::duration latency(init_target + (rand() % 50 + 1)); // always greater than target latency
        coDel.register_queue_latency(latency);
        mono_clock::duration timeout(10);
        std::this_thread::sleep_for(timeout);
    }
    ASSERT_FALSE(coDel.test_no_violation_count);
    ASSERT_TRUE(coDel.test_violation_count);
    ASSERT_EQ(coDel.test_violation_count, coDel.get_violation_count());
    auto final_interval = 50;
    for(auto i=1; i <= coDel.test_violation_count; i++){
        final_interval /= std::sqrt(i);
        if(final_interval <= 0){
            final_interval = 1;
        }
    }
    ASSERT_EQ(coDel.get_interval().count(), final_interval);
}