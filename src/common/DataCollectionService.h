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
#include <functional>

namespace fs = std::filesystem;

class DataCollectionService{
protected:
    std::string log_path;
    std::atomic<bool> active{false};
    std::atomic<bool> shutdown_flag{false};
    std::thread sys_state_thread;
    std::unordered_map<std::string, std::vector<uint64_t>> entries;
    std::unordered_map<std::string, std::vector<uint64_t>> ops;
    std::mutex entryMutex;
    std::atomic<int> entryCounter{0};
    static inline std::unique_ptr<DataCollectionService> _instance = nullptr;

    void logEntries() {
      boost::uuids::uuid u = boost::uuids::random_generator()();
      std::string uid = boost::uuids::to_string(u);
      std::ofstream entryFile(log_path + "entries_" + uid + ".csv");
      std::ofstream opFile(log_path + "ops_" + uid + ".csv");

      if (!entryFile.is_open() || !opFile.is_open()) {
        std::cerr << "Error: Failed to open log files at " << log_path << std::endl;
        return;
      }

      save(entryFile, entries);
      save(opFile, ops);

      entryFile.close();
      opFile.close();
    }

    static void save(std::ofstream &file, std::unordered_map<std::string, std::vector<uint64_t>> &data){
      // Writing headers (keys)
      bool first = true;
      for (const auto& pair : data) {
        if (!first) file << ",";
        file << pair.first;
        first = false;
      }
      file << std::endl;

      // Find the maximum vector size
      size_t maxSize = 0;
      for (const auto& pair : data) {
        maxSize = std::max(maxSize, pair.second.size());
      }

      // Writing row-wise data
      for (size_t i = 0; i < maxSize; ++i) {
        first = true;
        for (const auto& pair : data) {
          if (!first) file << ",";
          if (i < pair.second.size()) {
            file << pair.second[i];  // Write value if exists
          } else {
            file << "NaN";  // Default to NaN if no value
          }
          first = false;
        }
        file << std::endl;
      }
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
    static DataCollectionService& getInstance(){
      if(!_instance){
        _instance = std::make_unique<DataCollectionService>("/users/esmaeil/data/");
      }
      return *_instance;
    }

    DataCollectionService(std::string path)
            : log_path(path) {}

    int newEntry(uint64_t recv_stamp, uint64_t enqueue_stamp, uint64_t dequeue_stamp, uint64_t dequeue_end_stamp, int ops_len, uint64_t data_len, uint64_t data_off, uint64_t owner, int type, int cost, unsigned priority){
      int currentCount = entryCounter.fetch_add(1, std::memory_order_relaxed);
      std::lock_guard<std::mutex> lock(entryMutex);
      entries["index"].push_back(currentCount);
      entries["recv_stamp"].push_back(recv_stamp);
      entries["enqueue_stamp"].push_back(enqueue_stamp);
      entries["dequeue_stamp"].push_back(dequeue_stamp);
      entries["dequeue_end_stamp"].push_back(dequeue_end_stamp);
      entries["ops_len"].push_back(ops_len);
      entries["data_len"].push_back(data_len);
      entries["data_off"].push_back(data_off);
      entries["owner"].push_back(owner);
      entries["type"].push_back(static_cast<uint64_t>(type));
      entries["cost"].push_back(static_cast<uint64_t>(cost));
      entries["priority"].push_back(static_cast<uint64_t>(priority));
      return currentCount;
    }

    void newOp(int index, int type, uint64_t len, uint64_t off){
      std::lock_guard<std::mutex> lock(entryMutex);
      ops["index"].push_back(index);
      ops["type"].push_back(static_cast<uint64_t>(type));
      ops["len"].push_back(len);
      ops["off"].push_back(off);
    }

    void stop(){
      shutdown_flag.store(true);
      active.store(false);
      if (sys_state_thread.joinable()) {
        sys_state_thread.join();
      }
    }

    void dump() {
      logEntries();
      entries.clear();
      ops.clear();
    }

    void start(){
      if(!active.load()) {
        shutdown_flag.store(false);
        active.store(true);
        sys_state_thread = std::thread(&DataCollectionService::system_state_loop,
                                     this);
      }
    }

    bool isActive(){
      return active.load();
    }
};


#endif //CEPH_DATACOLLECTIONSERVICE_H
