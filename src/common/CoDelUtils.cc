#include "CoDelUtils.h"

double CoDelUtils::estimate_slope_by_regression(std::vector <DataPoint> &data_points) {
    double X_mean = 0;
    double Y_mean = 0;
    double SS_xy = 0;
    double SS_xx = 0;
    double multiply_sum = 0;
    double time_square_sum = 0;
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

double CoDelUtils::calculate_mean_and_std_dev(std::vector<double> &data_points, double *results) {
    int n = data_points.size();
    double mean = 0;
    for (int i = 0; i < data_points.size(); i++)
        mean += data_points[i];
    mean /= n;
    double standard_deviation = 0;
    for (int i = 0; i < data_points.size(); i++) {
        auto diff = data_points[i] - mean;
        standard_deviation += diff * diff;
    }
    standard_deviation /= n;
    standard_deviation = std::sqrt(standard_deviation);
    results[0] = mean;
    results[1] = standard_deviation;
}

void CoDelUtils::reject_outlier(std::vector<DataPoint> &data_points) {
    int n = data_points.size();
    double mean = 0;
    for (int i = 0; i < data_points.size(); i++)
        mean += data_points[i].value;
    mean /= n;
    double standard_deviation = 0;
    for (int i = 0; i < data_points.size(); i++) {
        auto diff = data_points[i].value - mean;
        standard_deviation += diff * diff;
    }
    standard_deviation /= n;
    standard_deviation = std::sqrt(standard_deviation);

    std::vector<int> to_be_removed;
    for (int i = 0; i < data_points.size(); i++) {
        double z_score = 0;
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

void CoDelUtils::getCofactor(double A[2][2], double temp[2][2], int p, int q, int n)
{
    int i = 0, j = 0;

    // Looping for each element of the matrix
    for (int row = 0; row < n; row++)
    {
        for (int col = 0; col < n; col++)
        {
            //  Copying into temporary matrix only those element
            //  which are not in given row and column
            if (row != p && col != q)
            {
                temp[i][j++] = A[row][col];

                // Row is filled, so increase row index and
                // reset col index
                if (j == n - 1)
                {
                    j = 0;
                    i++;
                }
            }
        }
    }
}

int CoDelUtils::determinant(double A[2][2], int n)
{
    double D = 0; // Initialize result

    //  Base case : if matrix contains single element
    if (n == 1)
        return A[0][0];

    double temp[2][2]; // To store cofactors

    int sign = 1;  // To store sign multiplier

    // Iterate for each element of first row
    for (int f = 0; f < n; f++)
    {
        // Getting Cofactor of A[0][f]
        getCofactor(A, temp, 0, f, n);
        D += sign * A[0][f] * determinant(temp, n - 1);

        // terms are to be added with alternate sign
        sign = -sign;
    }

    return D;
}

void CoDelUtils::adjoint(double A[2][2],double adj[2][2])
{

    // temp is used to store cofactors of A[][]
    int sign = 1;
    double temp[2][2];

    for (int i=0; i<2; i++)
    {
        for (int j=0; j<2; j++)
        {
            // Get cofactor of A[i][j]
            getCofactor(A, temp, i, j, 2);

            // sign of adj[j][i] positive if sum of row
            // and column indexes is even.
            sign = ((i+j)%2==0)? 1: -1;

            // Interchanging rows and columns to get the
            // transpose of the cofactor matrix
            adj[j][i] = (sign)*(determinant(temp, 1));
        }
    }
}

bool CoDelUtils::inverse(double A[2][2], double inverse[2][2])
{
    // Find determinant of A[][]
    double det = determinant(A, 2);
    if (det == 0)
    {
        std::cout << "Singular matrix, can't find its inverse";
        return false;
    }

    // Find adjoint
    double adj[2][2];
    adjoint(A, adj);

    // Find Inverse using formula "inverse(A) = adj(A)/det(A)"
    for (int i=0; i<2; i++)
        for (int j=0; j<2; j++)
            inverse[i][j] = adj[i][j]/det;

    return true;
}

void CoDelUtils::log_fit(std::vector<double> x, std::vector<double> y, double theta[2]) {
    int n = x.size();
    std::vector<double> x_log;
    x_log.reserve(n);
    for (int i = 0; i < n; i++)
        x_log.push_back(std::log(x[i]));
    double x_new[n][2];
    double x_new_t[2][n];
    for (int i = 0; i < n; i++) {
        x_new[i][0] = 1;
        x_new_t[0][i] = 1;
        x_new[i][1] = x_log[i];
        x_new_t[1][i] = x_log[i];
    }

    double x_new_t_dot_x_new[2][2] = {{0, 0},
                                      {0, 0}};
    int i;
    for (i = 0; i < n; i++) {
        x_new_t_dot_x_new[0][0] += x_new[i][0] * x_new_t[0][i];
        x_new_t_dot_x_new[0][1] += x_new[i][0] * x_new_t[1][i];
        x_new_t_dot_x_new[1][0] += x_new[i][1] * x_new_t[0][i];
        x_new_t_dot_x_new[1][1] += x_new[i][1] * x_new_t[1][i];
    }

    double temp_1[2][2];
    inverse(x_new_t_dot_x_new, temp_1);

    double temp_2[2][1] = {{0}, {0}};

    for (i = 0; i < n; i++) {
        temp_2[0][0] += x_new_t[0][i] * y[i];
        temp_2[1][0] += x_new_t[1][i] * y[i];
    }
    theta[0] = temp_1[0][0] * temp_2[0][0] + temp_1[0][1] * temp_2[1][0];
    theta[1] = temp_1[1][0] * temp_2[0][0] + temp_1[1][1] * temp_2[1][0];
}