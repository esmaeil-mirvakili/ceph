#include "CoDelUtils.h"

double_t CoDelUtils::estimate_slope_by_regression(vector <DataPoint> &data_points) {
    double_t X_mean = 0;
    double_t Y_mean = 0;
    double_t SS_xy = 0;
    double_t SS_xx = 0;
    double_t multiply_sum = 0;
    double_t time_square_sum = 0;
    int n = data_points.size();
    for (int i = 0; i < data_points.size(); i++) {
        X_mean += data_points[i].time;
        Y_mean += data_points[i].value;
        multiply_sum += (data_points[i].time * data_points[i].value);
        time_square_sum += (data_points[i].time * data_points[i].time);
    }
    X_mean /= n;
    Y_mean /= n;
    SS_xy = multiply_sum - (n * X_mean * Y_mean);
    SS_xx = time_square_sum - (n * X_mean * X_mean);
    if (SS_xx == 0)
        return 999999;
    return SS_xy / SS_xx;
}

void CoDelUtils::reject_outlier(vector<DataPoint> &data_points) {
    int n = data_points.size();
    double_t mean = 0;
    for (int i = 0; i < data_points.size(); i++)
        mean += data_points[i].value;
    mean /= n;
    double_t standard_deviation = 0;
    for (int i = 0; i < data_points.size(); i++) {
        auto diff = data_points[i].value - mean;
        standard_deviation += diff * diff;
    }
    standard_deviation /= n;
    standard_deviation = std::sqrt(standard_deviation);

    vector<int> to_be_removed;
    for (int i = 0; i < data_points.size(); i++) {
        double_t z_score = 0;
        if(standard_deviation != 0)
            z_score = (data_points[i].value - mean) / standard_deviation;
        if (std::abs(z_score) < 2)
            to_be_removed.push_back(i);
    }

    for (std::vector<int>::iterator it = to_be_removed.begin() ; it < to_be_removed.end(); ++it) {
        auto position = data_points.begin() + *it;
        data_points.erase(position);
    }
}