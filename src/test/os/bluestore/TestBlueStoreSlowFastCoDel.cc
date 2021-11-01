// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include <filesystem>
#include <iostream>
#include <unistd.h>
#include <mutex>
#include <cmath>
#include <vector>
#include <condition_variable>

#include "gtest/gtest.h"
#include "include/Context.h"
#include "include/rados/librados.hpp"
#include "include/rbd/librbd.hpp"
#include "librbd/ImageCtx.h"
#include "test/librados/test.h"
#include "global/global_init.h"
#include "global/global_context.h"
#include "test/librados/test_cxx.h"

#include "common/ceph_time.h"
#include "os/bluestore/BlueStoreSlowFastCoDel.h"

static int64_t milliseconds_to_nanoseconds(int64_t ms) {
  return ms * 1000.0 * 1000.0;
}

class BlueStoreSlowFastCoDelMock : public BlueStoreSlowFastCoDel {
public:
  BlueStoreSlowFastCoDelMock(
    CephContext *_cct,
    std::function<void(int64_t)> _bluestore_budget_reset_callback,
    std::function<int64_t()> _get_kv_throttle_current,
    std::mutex &_iteration_mutex,
    std::condition_variable &_iteration_cond,
    int64_t _target_latency,
    int64_t _fast_interval,
    int64_t _slow_interval,
    double _target_slope
    ): BlueStoreSlowFastCoDel(_cct, _bluestore_budget_reset_callback, _get_kv_throttle_current),
    iteration_mutex(_iteration_mutex), iteration_cond(_iteration_cond),
    test_target_latency(_target_latency), test_target_slope(_target_slope),
    test_fast_interval(_fast_interval), test_slow_interval(_slow_interval){
    on_config_changed(_cct);
  }

  void on_config_changed(CephContext *cct) {
    std::lock_guard l(register_lock);

    activated = true;
    target_slope = test_target_slope;
    slow_interval = test_slow_interval;
    initial_fast_interval = test_fast_interval;
    min_target_latency = milliseconds_to_nanoseconds(500);
    initial_target_latency = test_target_latency;
    max_target_latency = milliseconds_to_nanoseconds(1);
    initial_bluestore_budget = 100 * 1024;
    min_bluestore_budget = 10 * 1024;
    bluestore_budget_increment = 1024;
    regression_history_size = 5;

    bluestore_budget = initial_bluestore_budget;
    min_bluestore_budget = initial_bluestore_budget;
    max_queue_length = min_bluestore_budget;
    fast_interval = initial_fast_interval;
    target_latency = initial_target_latency;
    min_latency = INITIAL_LATENCY_VALUE;
    slow_interval_registered_bytes = 0;
    regression_throughput_history.clear();
    regression_target_latency_history.clear();
    slow_interval_start = ceph::mono_clock::zero();
  }

  std::vector<int64_t> target_latency_vector;

protected:
  std::mutex &iteration_mutex;
  std::condition_variable &iteration_cond;
  int64_t test_target_latency;
  int64_t test_fast_interval;
  int64_t test_slow_interval;
  double test_target_slope;

  void on_fast_interval_finished() {
    BlueStoreSlowFastCoDel::on_fast_interval_finished();
    std::unique_lock<std::mutex> locker(iteration_mutex);
    iteration_cond.notify_one();
  }

  void on_slow_interval_finished() {
    BlueStoreSlowFastCoDel::on_slow_interval_finished();
    std::vector<int> x;
    target_latency_vector.push_back(target_latency);
  }
};

class TestSlowFastCoDel : public ::testing::Test {
public:
  librados::Rados* test_rados = nullptr;
  CephContext* ceph_context = nullptr;
  librados::IoCtx io_ctx;
  std::string temp_pool_name;
  BlueStoreSlowFastCoDel* slow_fast_codel = nullptr;
  int64_t test_throttle_budget = 0;
  std::mutex iteration_mutex;
  std::condition_variable iteration_cond;
  int64_t target_latency = milliseconds_to_nanoseconds(50);
  int64_t fast_interval = milliseconds_to_nanoseconds(10);
  int64_t slow_interval = milliseconds_to_nanoseconds(100);
  double target_slope = 1;

  TestSlowFastCoDel(){}

  ~TestSlowFastCoDel(){}

  static void SetUpTestCase() {}
  static void TearDownTestCase() {}

  void SetUp() override {
    test_rados = new librados::Rados();
    ASSERT_EQ("", connect_cluster_pp(*test_rados));
  }

  void create_bluestore_slow_fast_codel() {
    temp_pool_name = get_temp_pool_name("test_pool_");
    ASSERT_EQ(0, test_rados->pool_create(temp_pool_name.c_str()));
    ASSERT_EQ(0, test_rados->ioctx_create(temp_pool_name.c_str(), io_ctx));
    ceph_context = reinterpret_cast<CephContext *>(test_rados->cct());
    slow_fast_codel = new BlueStoreSlowFastCoDelMock(ceph_context,
                                                 [this](int64_t x) mutable {
                                                   this->test_throttle_budget += x;
                                                 },
                                                 [this]() mutable {
                                                   return this->test_throttle_budget;
                                                 },
                                                 iteration_mutex,
                                                 iteration_cond,
                                                 target_latency,
                                                 fast_interval,
                                                 slow_interval,
                                                 target_slope);
  }

  void TearDown() override {
    if(test_rados)
      delete test_rados;
    if(slow_fast_codel)
      delete slow_fast_codel;
  }

  int no_violation_time_diff[4] = {-15, 20, -20, 10};
  int violation_time_diff[4] = {30, 25, 10, 15};

  void test_codel() {
    int64_t max_iterations = 10;
    int iteration_timeout = 10; // 10 sec
    for (int iteration = 0; iteration < max_iterations; iteration++) {
      std::unique_lock <std::mutex> locker(iteration_mutex);
      bool violation = std::rand() % 2 == 0;
      auto budget_tmp = test_throttle_budget;
      auto target = slow_fast_codel->get_target_latency();
      int64_t txc_size = (target_slope * target_latency) * std::log(target * 1.0) / 4;

      for (int i = 0; i < 4; i++) {
        auto now = ceph::mono_clock::now();
        auto time_diff = violation ? violation_time_diff[i]
                                   : no_violation_time_diff[i];
        time_diff = milliseconds_to_nanoseconds(time_diff);
        auto time = now - std::chrono::nanoseconds(target + time_diff);
        slow_fast_codel->update_from_txc_info(time, txc_size);
      }
      iteration_cond.wait(locker);  // wait for fast or slow loop to finish
      if (iteration_cond.wait_for(
        locker,
        std::chrono::seconds(iteration_timeout)) == std::cv_status::timeout) {
        ASSERT_TRUE(false);
        return;
      }
      if (violation) {
        ASSERT_LT(test_throttle_budget, budget_tmp);
      } else {
        ASSERT_GT(test_throttle_budget, budget_tmp);
      }
    }
  }
};

TEST_F(TestSlowFastCoDel, test1) {
  create_bluestore_slow_fast_codel();
  test_codel();
}
