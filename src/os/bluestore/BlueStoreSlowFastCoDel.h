// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-

#pragma once

#include <iostream>
#include <atomic>
#include <ostream>
#include <fstream>
#include <string>

#include "include/Context.h"
#include "common/Timer.h"
#include "include/utime.h"
#include "common/Thread.h"
#include "common/ceph_time.h"
#include "common/admin_socket.h"
#include "common/Formatter.h"

using ceph::bufferlist;
using ceph::Formatter;

class BlueStoreSlowFastCoDel
{
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

protected:
  static const int64_t INITIAL_LATENCY_VALUE = -1;

  bool activated = false;
  int64_t bluestore_budget = 102400;
  int64_t min_latency = INITIAL_LATENCY_VALUE;
  int64_t prev_latency = INITIAL_LATENCY_VALUE;
  ceph::mono_clock::time_point prev_ts = ceph::mono_clock::zero();
  double gamma = 0.5;
  int64_t beta = 102400;
  int64_t min_bluestore_budget = 102400;
  int64_t initial_bluestore_budget = 102400;

  std::function<void(int64_t)> bluestore_budget_reset_callback;
  std::function<int64_t(void)> get_kv_throttle_current;

private:
  template <typename T>
  double millisec_to_nanosec(T ms)
  {
    return ms * 1000.0 * 1000.0;
  }

  template <typename T>
  double nanosec_to_millisec(T ns)
  {
    return ns / (1000.0 * 1000.0);
  }

  template <typename T>
  double nanosec_to_sec(T ns)
  {
    return ns / (1000.0 * 1000.0 * 1000.0);
  }
};