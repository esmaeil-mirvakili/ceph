#include <iostream>
#include <vector>
#include <cmath>
#include <boost/numeric/ublas/matrix.hpp>

#define Z_P 2.33  // z score for 99th percentile

class RegressionUtils {
public:
    static boost::numeric::ublas::matrix<double> matrix_inverse(boost::numeric::ublas::matrix<double>& m);
    static void logarithmic_regression(std::vector<double> x_values, std::vector<double> y_values, double theta[2]);
    static double find_slope_on_logarithmic_curve(std::vector<double> x_values, std::vector<double> y_values, double target_slope);
    static void find_log_normal_dist_params(double mode, double min_x, double max_x, double params[2]);
};
