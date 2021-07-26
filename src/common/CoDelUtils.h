#ifndef CEPH_CODELUTILS_H
#define CEPH_CODELUTILS_H

#include <iostream>
#include <vector>
#include <cmath>
#include "ceph_time.h"

#define N 2

using ceph::mono_clock;

class CoDelUtils {
public:
    struct DataPoint{
        double time;
        double value;
//        mono_clock::time_point created;
    };
    static double estimate_slope_by_regression(std::vector<DataPoint> &data_points);
    static void reject_outlier(std::vector<DataPoint> &data_points);

#ifdef N
    static void getCofactor(double A[N][N], double temp[N][N], int p, int q, int n);
    static int determinant(double A[N][N], int n);
    static void adjoint(double A[N][N],double adj[N][N]);
    static bool inverse(double A[N][N], double inverse[N][N]);
    static void log_fit(std::vector<double> x, std::vector<double> y, double theta[2]);
#endif
};


#endif //CEPH_CODELUTILS_H
