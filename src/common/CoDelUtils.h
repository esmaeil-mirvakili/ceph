#ifndef CEPH_CODELUTILS_H
#define CEPH_CODELUTILS_H

#include <iostream>
#include <vector>
#include "ceph_time.h"

class CoDelUtils {
public:
    static struct DataPoint{
        double time;
        double value;
        mono_clock::time_point created;
    };
    static double_t estimate_slope_by_regression(vector<DataPoint> &data_points);
    static void reject_outlier(vector<DataPoint> &data_points);
};


#endif //CEPH_CODELUTILS_H
