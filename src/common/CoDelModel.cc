#include "CoDelModel.h"

LatencyRange::LatencyRange(int64_t start_time, int64_t range, bool outlier_detection, int max_size, int64_t ttl)
    : start_time(start_time), range(range), max_size(max_size), ttl(ttl), outlier_detection(outlier_detection)  {
    outfile.open("log3.log");
}

LatencyRange::LatencyRange(int64_t start_time, int64_t range, bool outlier_detection, int max_size)
        : LatencyRange(start_time, range, outlier_detection, max_size, 0)  {}

LatencyRange::LatencyRange(int64_t start_time, int64_t range, bool outlier_detection)
        : LatencyRange(start_time, range, outlier_detection, 0)  {}

LatencyRange::~LatencyRange() {}

void LatencyRange::reset() {
    time_series.clear();
    slope = 0;
}

void LatencyRange::add_point(double latency, double throughput) {
    *outfile << "2_2_1" << std::endl;
    outfile->flush();
    mono_clock::time_point now = mono_clock::now();
    DataPoint data_point;
    *outfile << "2_2_2" << std::endl;
    outfile->flush();
    data_point.time = latency / 1000000;
    data_point.value = throughput;
    data_point.created = now;
    *outfile << "2_2_3" << std::endl;
    outfile->flush();
    time_series.push_back(data_point);
    *outfile << "2_2_4" << std::endl;
    outfile->flush();
    if (max_size > 0 && time_series.size() > max_size)
        time_series.erase(time_series.begin());
    *outfile << "2_2_5" << std::endl;
    outfile->flush();
}

void LatencyRange::update_slope() {
    auto temp = time_series;
    if(outlier_detection)
        CoDelUtils::reject_outlier(temp);
    slope = CoDelUtils::estimate_slope_by_regression(temp);
}

void LatencyRange::clean() {
    if(ttl > 0) {
        mono_clock::time_point now = mono_clock::now();
        std::vector<DataPoint>::iterator it = time_series.begin();
        while (it != time_series.end()) {
            if (std::chrono::nanoseconds(now - (*it).created).count() > ttl)
                it = time_series.erase(it);
            else
                ++it;
        }
    }
}

double LatencyRange::get_slope() {
    clean();
    update_slope();
    return slope;
}

int LatencyRange::get_size() {
    return time_series.size();
}

int64_t LatencyRange::get_start_time() {
    return start_time;
}

int64_t LatencyRange::get_range() {
    return range;
}

CoDelModel::CoDelModel(int64_t min_latency ,int64_t max_latency, int64_t interval, int64_t config_latency_threshold, bool outlier_detection, int threshold)
    : min_latency(min_latency), max_latency(max_latency), interval(interval), config_latency_threshold(config_latency_threshold), outlier_detection(outlier_detection) {
    outfile.open("log2.log");
    std::ofstream * LatencyRange::outfile = NULL;
    std::ofstream out("log3.log");
    LatencyRange::outfile = &out;
    size = int(std::ceil((max_latency - min_latency) / interval));
    latency_ranges = (LatencyRange*)malloc(sizeof(LatencyRange) * size);;
    for(int i = 0; i < size; i++)
        latency_ranges[i] = LatencyRange(min_latency + i * interval, interval, outlier_detection);
    srand(time(0));
    if(threshold > 0)
        size_threshold = threshold;
}

CoDelModel::CoDelModel(int64_t min_latency ,int64_t max_latency, int64_t interval, int64_t config_latency_threshold, bool outlier_detection)
    : CoDelModel(min_latency, max_latency, interval, config_latency_threshold, outlier_detection, -1) {}

CoDelModel::~CoDelModel() {
    outfile.close();
    delete latency_ranges;
}

void CoDelModel::reset() {
    for(int i = 0; i < size; i++)
        latency_ranges[i].reset();
}

int CoDelModel::get_index(double latency) {
    return int(std::floor((latency - min_latency)/interval));
}

void CoDelModel::add_point(double latency, double throughput) {
    outfile << "2_1" << std::endl;
    outfile.flush();
    int index = get_index(latency);
    outfile << "2_2" << std::endl;
    outfile.flush();
    latency_ranges[index].add_point(latency, throughput);
    outfile << "2_3" << std::endl;
    outfile.flush();
}

double CoDelModel::get_latency_for_slope(double latency, double threshold_slope) {
    int index = get_index(latency);
    if(latency_ranges[index].get_size() < size_threshold){
        double offset = (rand() % 10) / 10.0;
        int range_start = min_latency + index * interval;
        return range_start + interval * offset;
    }
    if(config_mode || latency_ranges[index].get_slope() >= threshold_slope) {
        int64_t new_latency = min_latency + (index + 1) * interval;
        if(config_mode && new_latency > config_latency_threshold){
            config_mode = false;
            new_latency = min_latency;
        }
        return std::min(new_latency, max_latency);
    }
    int64_t new_latency = min_latency + (index - 1) * interval;
    return std::max(new_latency, min_latency);
}

void CoDelModel::get_slope(double latency, double *slope) {
    int index = get_index(latency);
    if(latency_ranges[index].get_size() > 2)
        *slope = latency_ranges[index].get_slope();
}

std::string CoDelModel::to_string() {
    std::string output = "{";
    for(int i = 0; i < size; i++) {
        output += std::to_string(latency_ranges[i].get_start_time());
        output += ": {";
        output += "\"range\": ";
        output += std::to_string(latency_ranges[i].get_range());
        output += ", \"slope\": ";
        output += latency_ranges[i].get_slope();
        output += ", \"size\": ";
        output += latency_ranges[i].get_size();
        output += "}, ";
    }
    output += "}";
}
