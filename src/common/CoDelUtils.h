#ifndef CEPH_CODELUTILS_H
#define CEPH_CODELUTILS_H

#include <iostream>
#include <vector>
#include "ceph_time.h"

using ceph::mono_clock;

class CoDelUtils {
public:
    struct DataPoint{
        double time;
        double value;
        mono_clock::time_point created;
    };
    static double estimate_slope_by_regression(std::vector<DataPoint> &data_points);
    static void reject_outlier(std::vector<DataPoint> &data_points);
};


#endif //CEPH_CODELUTILS_H
