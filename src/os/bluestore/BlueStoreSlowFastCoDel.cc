// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-

#include "BlueStoreSlowFastCoDel.h"

#include "common/regression_utils.h"

BlueStoreSlowFastCoDel::BlueStoreSlowFastCoDel(
    CephContext *_cct,
    std::function<void(int64_t)> _bluestore_budget_reset_callback,
    std::function<int64_t()> _get_kv_throttle_current) : bluestore_budget_reset_callback(_bluestore_budget_reset_callback),
                                                         get_kv_throttle_current(_get_kv_throttle_current)
{
  on_config_changed(_cct);
}

BlueStoreSlowFastCoDel::~BlueStoreSlowFastCoDel()
{
}

void BlueStoreSlowFastCoDel::update_from_txc_info(
    ceph::mono_clock::time_point txc_start_time,
    uint64_t txc_bytes)
{
  if (!activated)
    return;
  ceph::mono_clock::time_point now = ceph::mono_clock::now();
  int64_t latency = std::chrono::nanoseconds(now - txc_start_time).count();
  if (min_latency == INITIAL_LATENCY_VALUE || latency < min_latency)
  {
    min_latency = latency;
  }
  if (prev_latency > 0)
  {
    ceph::mono_clock::time_point now = ceph::mono_clock::now();
    int64_t dt = std::chrono::nanoseconds(txc_start_time - prev_ts).count();
    double theta = (latency - prev_latency) / (dt * 1.0);
    double pow_norm = (latency * (theta + 1)) / min_latency;
    bluestore_budget = gamma * ((bluestore_budget / pow_norm) + beta) + (1 - gamma) * bluestore_budget;
    reset_bluestore_budget();
  }
  prev_latency = latency;
  prev_ts = txc_start_time;
}

void BlueStoreSlowFastCoDel::on_config_changed(CephContext *cct)
{
  {
    activated = cct->_conf->bluestore_codel;
    min_latency = INITIAL_LATENCY_VALUE;
    prev_latency = INITIAL_LATENCY_VALUE;
    prev_ts = ceph::mono_clock::now();
    gamma = cct->_conf->bluestore_codel_gamma;
    beta = cct->_conf->bluestore_codel_beta;
    min_bluestore_budget = cct->_conf->bluestore_codel_min_budget_bytes;
    initial_bluestore_budget = cct->_conf->bluestore_codel_initial_budget_bytes;
    bluestore_budget = initial_bluestore_budget;
  }
}

void BlueStoreSlowFastCoDel::reset_bluestore_budget()
{
  if (activated)
  {
    bluestore_budget = std::max(min_bluestore_budget, bluestore_budget);
    bluestore_budget_reset_callback(bluestore_budget);
  }
}

int64_t BlueStoreSlowFastCoDel::get_bluestore_budget()
{
  return bluestore_budget;
}

int64_t BlueStoreSlowFastCoDel::get_target_latency()
{
  return target_latency;
}
