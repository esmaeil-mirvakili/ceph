#ifndef CEPH_CODELMODEL_H
#define CEPH_CODELMODEL_H

#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <stdlib.h>
#include <time.h>
#include <fstream>
#include "ceph_time.h"
#include "CoDelUtils.h"

using ceph::mono_clock;
using DataPoint = CoDelUtils::DataPoint;

class LatencyRange{
public:
    LatencyRange(int64_t start_time, int64_t range, bool outlier_detection, int max_size, int64_t ttl);
    LatencyRange(int64_t start_time, int64_t range, bool outlier_detection, int max_size);
    LatencyRange(int64_t start_time, int64_t range, bool outlier_detection);
    ~LatencyRange();

    void reset();
    void add_point(double latency, double throughput);
    double get_slope();
    int get_size();
    int64_t get_start_time();
    int64_t get_range();
    static std::ofstream outfile;
private:
    void update_slope();
    void clean();

    int64_t start_time;    // in ns
    int64_t range;    // in ns
    int max_size = 0;
    int64_t ttl = 0;    // in ns
    double_t slope;
    std::vector<DataPoint> time_series;
    bool outlier_detection = false;
};

class CoDelModel {
public:
    CoDelModel(int64_t min_latency ,int64_t max_latency, int64_t interval, int64_t config_latency_threshold, bool outlier_detection, int threshold);
    CoDelModel(int64_t min_latency ,int64_t max_latency, int64_t interval, int64_t config_latency_threshold, bool outlier_detection);
    ~CoDelModel();

    void reset();
    double get_latency_for_slope(double latency, double threshold_slope);
    void get_slope(double latency, double *slope);
    void add_point(double latency, double throughput);
    std::string to_string();
    std::ofstream outfile;
private:
    LatencyRange* latency_ranges;
    int64_t min_latency;
    int64_t max_latency;
    int64_t interval;
    int size;
    int size_threshold = 20;
    bool config_mode = true;
    int64_t config_latency_threshold = 10;
    bool outlier_detection = false;

    int get_index(double latency);
};


#endif //CEPH_CODELMODEL_H
