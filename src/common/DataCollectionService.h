//
// Created by Esmaeil on 2/3/25.
//

#ifndef CEPH_DATACOLLECTIONSERVICE_H
#define CEPH_DATACOLLECTIONSERVICE_H

#include <atomic>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/thread/shared_mutex.hpp>

#include "common/Thread.h"
#include "common/Clock.h"

#include <fstream>
#include <string>
#include <iostream>
#include <map>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;

struct DataCollectionRequestInfo {
    uint64_t recv_stamp = 0;
    uint64_t enqueue_stamp = 0;
    uint64_t dequeue_stamp = 0;
    uint64_t commit_stamp = 0;
    uint64_t dequeue_end_stamp = 0;
    uint64_t data_len = 0;
    uint64_t data_off = 0;
    uint64_t owner = 0;
    int type = 0;
    int cost = 0;
    unsigned priority = 0;
    uint64_t bluestore_bytes = 0;
    uint64_t bluestore_ios = 0;
    uint64_t bluestore_cost = 0;
    int64_t throttle_current = 0;
    int64_t throttle_max = 0;

    DataCollectionRequestInfo &operator=(const DataCollectionRequestInfo &other) {
      if (this != &other) {
        recv_stamp = other.recv_stamp;
        enqueue_stamp = other.enqueue_stamp;
        dequeue_stamp = other.dequeue_stamp;
        dequeue_end_stamp = other.dequeue_end_stamp;
        data_len = other.data_len;
        data_off = other.data_off;
        owner = other.owner;
        type = other.type;
        cost = other.cost;
        priority = other.priority;
      }
      return *this;
    }

    void print(std::ofstream &ss) const {
      ss << recv_stamp << ", ";
      ss << enqueue_stamp << ", ";
      ss << dequeue_stamp << ", ";
      ss << dequeue_end_stamp << ", ";
      ss << data_len << ", ";
      ss << data_off << ", ";
      ss << owner << ", ";
      ss << type << ", ";
      ss << cost << ", ";
      ss << priority;
    }
};

class DataEntry {
public:
    std::string id;
    DataCollectionRequestInfo reqInfo;

    void log(std::ofstream &entryStream) {
      entryStream << id;
      entryStream << ", ";
      reqInfo.print(entryStream);
      entryStream << std::endl;
    }

public:
    DataEntry() {
      boost::uuids::uuid u = boost::uuids::random_generator()();
      id = boost::uuids::to_string(u);
    }

    DataCollectionRequestInfo &getReqInfo() {
      return reqInfo;
    }

    DataEntry &operator=(const DataEntry &other) {
      if (this != &other) {
        id = other.id;
        reqInfo = other.reqInfo;
      }
      return *this;
    }

    friend class DataCollectionService;
};

class DataCollectionService{
protected:
    std::string log_path;
    std::vector <DataEntry> entries;
    std::atomic<bool> active{false};
    std::atomic<bool> shutdown_flag{false};

    bool load_disk_paths(const std::string &file_path, std::string &ssd_disk, std::string &hdd_disk) {
      std::ifstream file(file_path);
      if (!file) {
        std::cerr << "Error opening disk info file: " << file_path << std::endl;
        return false;
      }
      if (!std::getline(file, ssd_disk) || !std::getline(file, hdd_disk)) {
        std::cerr << "Invalid disk info file format." << std::endl;
        return false;
      }
      file.close();
      return true;
    }

    void logEntries() {
      boost::uuids::uuid u = boost::uuids::random_generator()();
      std::string uid = boost::uuids::to_string(u);
      std::ofstream entryFile(log_path + "entries_" + uid + ".csv");

      if (!entryFile.is_open()) {
        std::cerr << "Error: Failed to open log files at " << log_path << std::endl;
        return;
      }

      entryFile << "id, recv_stamp, enqueue_stamp, dequeue_stamp, dequeue_end_stamp, data_len, data_off, owner, type, cost, priority" << std::endl;

      for (auto &entry: entries) {
        entry.log(entryFile);
      }
      entryFile.close();
    }

    void copy_file(const std::string &file_path, fs::path &destination_folder, const std::string &name){
      fs::path dest_path = destination_folder / name;
      try {
        fs::copy_file(file_path, dest_path, fs::copy_options::overwrite_existing);
      } catch (const std::exception &e) {
        std::cerr << "Error copying file: " << e.what() << std::endl;
      }
    }

    void capture_system_state() {
      uint64_t now = ceph_clock_now().to_nsec();
      fs::path destination_folder = fs::path(log_path) / (std::to_string(now) + "/");
      if (!fs::exists(destination_folder)) {
        fs::create_directories(destination_folder);
      }
      copy_file("/proc/diskstats", destination_folder, "disk_stats.txt");
      copy_file("/proc/meminfo", destination_folder, "mem.txt");
      copy_file("/proc/stat", destination_folder, "cpu.txt");
    }

    void system_state_loop(){
      while (!shutdown_flag.load()) {
        if(active.load())
          capture_system_state();
        std::this_thread::sleep_for(std::chrono::seconds(5));
      }
    }
public:
    DataCollectionService(std::string path)
            : log_path(path) {}

    void newEntry(DataEntry &entry) {
      if(!active.load())
        return;
      DataEntry newEntry = entry;
      entries.push_back(newEntry);
    }

    void stop(){
      shutdown_flag.store(true);
      active.store(false);
    }

    void dump() {
      logEntries();
      entries.clear();
    }

    void start(){
      shutdown_flag.store(false);
      active.store(true);
      std::thread sys_state_thread(&DataCollectionService::system_state_loop, this);
      sys_state_thread.detach();
    }
};


#endif //CEPH_DATACOLLECTIONSERVICE_H
