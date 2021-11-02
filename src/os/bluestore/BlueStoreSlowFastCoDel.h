// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef CEPH_OS_BLUESTORE_SLOWFASTCODEL_H
#define CEPH_OS_BLUESTORE_SLOWFASTCODEL_H

#include <iostream>

#include "include/Context.h"
#include "common/Timer.h"
#include "common/ceph_time.h"

class BlueStoreSlowFastCoDel {
public:
  BlueStoreSlowFastCoDel(
    CephContext *_cct,
    std::function<void(int64_t)> _bluestore_budget_reset_callback,
    std::function<int64_t()> _get_kv_throttle_current);

  virtual ~BlueStoreSlowFastCoDel();

  void on_config_changed(CephContext *cct);

  void reset_bluestore_budget();

  void update_from_txc_info(
    ceph::mono_clock::time_point txc_start_time,
    uint64_t txc_bytes);

  int64_t get_bluestore_budget();

  int64_t get_target_latency();

  bool is_activated();

protected:
  static const int64_t INITIAL_LATENCY_VALUE = -1;
  // true if SlowFastCodel is activated
  bool activated = false;
  // Initial interval for fast loop (original codel)
  int64_t initial_fast_interval = INITIAL_LATENCY_VALUE;
  // Initial target latency to start the algorithm
  int64_t initial_target_latency = INITIAL_LATENCY_VALUE;
  // the interval for the slow loop
  int64_t slow_interval = INITIAL_LATENCY_VALUE;
  // current interval for the fast loop
  int64_t fast_interval = INITIAL_LATENCY_VALUE;
  // current target latency that fast loop is using
  int64_t target_latency = INITIAL_LATENCY_VALUE;
  // min latency in the current fast interval
  int64_t min_latency = INITIAL_LATENCY_VALUE;
  int64_t min_target_latency = INITIAL_LATENCY_VALUE;  // in ns
  int64_t max_target_latency = INITIAL_LATENCY_VALUE; // in ns
  int64_t violation_count = 0;
  ceph::mutex fast_timer_lock = ceph::make_mutex("CoDel::fast_timer_lock");
  ceph::mutex slow_timer_lock = ceph::make_mutex("CoDel::slow_timer_lock");
  ceph::mutex register_lock = ceph::make_mutex("CoDel::register_lock");
  SafeTimer fast_timer;  // fast loop timer
  SafeTimer slow_timer;  // slow loop timer
  // marks the start of the current slow interval
  ceph::mono_clock::time_point slow_interval_start = ceph::mono_clock::zero();
  // define the tradeoff between throughput and latency (MB/s loss for every
  // 1ms decrease in latency)
  double target_slope = 5;
  // amount of bytes that has been processed in current slow interval
  int64_t slow_interval_registered_bytes = 0;
  // number of transactions that has been processed in current slow interval
  int64_t slow_interval_txc_cnt = 0;
  // target latency history for regression
  std::vector<double> regression_target_latency_history;
  // throughput history for regression
  std::vector<double> regression_throughput_history;
  int64_t regression_history_size = 100;  // regression history size
  // the minimum bluestore throttle budget
  int64_t min_bluestore_budget = 102400;
  // the initial bluestore throttle budget
  int64_t initial_bluestore_budget = 102400;
  // the increment size for opening the bluestore throttle
  int64_t bluestore_budget_increment = 102400;
  int64_t bluestore_budget = 102400;  // current bluestore throttle budget
  // maximum amount of inflight data in current slow interval
  int64_t max_queue_length = 102400;
  std::function<void(int64_t)> bluestore_budget_reset_callback;
  std::function<int64_t(void)> get_kv_throttle_current;

  void on_min_latency_violation();

  void on_no_violation();

  virtual void on_fast_interval_finished() {}

  virtual void on_slow_interval_finished() {}

private:

  bool _check_latency_violation();

  void _update_interval();

  void _fast_interval_process();

  void _slow_interval_process();

  template<typename T>
  double millisec_to_nanosec(T ms) {
    return ms * 1000.0 * 1000.0;
  }

  template<typename T>
  double nanosec_to_millisec(T ns) {
    return ns / (1000.0 * 1000.0);
  }

  template<typename T>
  double nanosec_to_sec(T ns) {
    return ns / (1000.0 * 1000.0 * 1000.0);
  }
};

#endif //CEPH_OS_BLUESTORE_SLOWFASTCODEL_H
