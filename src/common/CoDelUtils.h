#ifndef CEPH_CODELUTILS_H
#define CEPH_CODELUTILS_H

#include <iostream>
#include <vector>
#include <cmath>
#include "ceph_time.h"

#define Z_P 3

using ceph::mono_clock;

class CoDelUtils {
public:
    struct DataPoint{
        double time;
        double value;
//        mono_clock::time_point created;
    };
    static double estimate_slope_by_regression(std::vector<DataPoint> &data_points);
    static double calculate_mean_and_std_dev(std::vector<double> &data_points, double results[2]);
    static void reject_outlier(std::vector<double> &x, std::vector<double> &y);
    static void getCofactor(double A[2][2], double temp[2][2], int p, int q, int n);
    static int determinant(double A[2][2], int n);
    static void adjoint(double A[2][2],double adj[2][2]);
    static bool inverse(double A[2][2], double inverse[2][2]);
    static void log_fit(std::vector<double> x, std::vector<double> y, double theta[2], bool outlier_detection);
    void find_log_normal_dist_params(double mode, double min_x, double max_x, double params[2]);
};


#endif //CEPH_CODELUTILS_H
