//
// Created by Esmaeil on 2/3/25.
//

#ifndef CEPH_DATACOLLECTIONSERVICE_H
#define CEPH_DATACOLLECTIONSERVICE_H

#include <atomic>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "common/ceph_mutex.h"
#include "common/Thread.h"
#include "common/Clock.h"

struct RequestInfo {
    uint64_t recv_stamp;
    uint64_t enqueue_stamp;
    uint64_t dequeue_stamp;
    uint64_t commit_stamp;
    uint64_t owner;
    int type;
    int cost;
    unsigned priority;
    uint64_t bluestore_bytes;
    uint64_t bluestore_ios;
    uint64_t bluestore_cost;
    int64_t throttle_current;
    int64_t throttle_max;

    RequestInfo &operator=(const RequestInfo &other) {
      if (this != &other) {
        recv_stamp = other.recv_stamp;
        enqueue_stamp = other.enqueue_stamp;
        dequeue_stamp = other.dequeue_stamp;
        commit_stamp = other.commit_stamp;
        owner = other.owner;
        type = other.type;
        cost = other.cost;
        priority = other.priority;
        bluestore_bytes = other.bluestore_bytes;
        bluestore_ios = other.bluestore_ios;
        bluestore_cost = other.bluestore_cost;
        throttle_current = other.throttle_current;
        throttle_max = other.throttle_max;
      }
      return *this;
    }

    std::string getHeader() {
      return "recv_stamp, enqueue_stamp, dequeue_stamp, commit_stamp, owner, type, cost, priority, bluestore_bytes, bluestore_ios, bluestore_cost, throttle_current, throttle_max";
    }

    std::string toString() const {
      std::stringstream ss;
      ss << recv_stamp << ", ";
      ss << enqueue_stamp << ", ";
      ss << dequeue_stamp << ", ";
      ss << commit_stamp << ", ";
      ss << owner << ", ";
      ss << type << ", ";
      ss << cost << ", ";
      ss << priority << ", ";
      ss << bluestore_bytes << ", ";
      ss << bluestore_ios << ", ";
      ss << bluestore_cost << ", ";
      ss << throttle_current << ", ";
      ss << throttle_max;
      return ss.str();
    }
};

struct OpInfo {
    uint32_t type;
    uint32_t cid;
    uint32_t oid;
    uint64_t off;
    uint64_t len;

    OpInfo(uint32_t _type, uint32_t _cid, uint32_t _oid, uint64_t _off,
           uint64_t _len) : type(_type), cid(_cid), oid(_oid), off(_off),
                            len(_len) {}

    std::string getHeader() {
      return "type, cid, oid, off, len";
    }

    std::string toString() const {
      std::stringstream ss;
      ss << type << ", ";
      ss << cid << ", ";
      ss << oid << ", ";
      ss << off << ", ";
      ss << len;
      return ss.str();
    }

};

class DataEntry {
protected:
    std::string id;
    RequestInfo reqInfo;
    std::vector <OpInfo> ops;

    void log(std::ofstream &entryStream, std::ofstream &opsStream) {
      entryStream << id << ", " << reqInfo.toString() << std::endl;
      std::for_each(ops.begin(), ops.end(), [&](OpInfo &opInfo) {
          opsStream << id << ", " << opInfo.toString() << std::endl;
      });
    }

public:
    DataEntry() {
      boost::uuids::uuid u = boost::uuids::random_generator()();
      id = boost::uuids::to_string(u);
    }

    const RequestInfo &getReqInfo() {
      return reqInfo;
    }

    void addOp(uint32_t type, uint32_t cid, uint32_t oid, uint64_t off,
               uint64_t len) {
      ops.push_back(OpInfo(type, cid, oid, off, len));
    }

    DataEntry &operator=(const DataEntry &other) {
      if (this != &other) {
        id = other.id;
        reqInfo = other.reqInfo;
        ops.clear();
        std::for_each(other.ops.begin(), other.ops.end(), [this](OpInfo &op) {
            this->addOp(op.type, op.cid, op.oid, op.off, op.len);
        });
      }
      return *this;
    }

    friend class DataCollectionService;
};

class DataCollectionServiceThread : public Thread {
protected:
    std::string log_path;
    int log_cap;
    int dumpDelay;
    std::vector <DataEntry> entries;
    int idx = 0;
    boost::shared_mutex rw_mutex;
    std::atomic<bool> writer_waiting = false;
    std::atomic<bool> post_write_delay = false;
    std::atomic<bool> shutdown_thread = false;

    void logEntries() {
      std::ofstream entryFile(log_path + "entries_" + idx + ".csv");
      std::ofstream opsFile(log_path + "ops_" + idx + ".csv");

      if (!entryFile.is_open() || !opsFile.is_open()) {
        std::cerr << "Error: Failed to open log files at " << log_path << std::endl;
        return;
      }

      idx++;
      for (auto &entry: entries) {
        entry.log(entryFile, opsFile);
      }
      entryFile.close();
      opsFile.close();
    }

    void dump() {
      // Indicate that a writer is waiting
      writer_waiting.store(true);

      // Acquire exclusive lock
      boost::unique_lock <boost::shared_mutex> lock(rw_mutex);

      // Now writer has the lock
      logEntries();
      entries.clear();

      // Activate post-write delay
      post_write_delay.store(true);
      writer_waiting.store(false);

      // Hold readers for dumpDelay seconds
      usleep(dumpDelay * 1000000);

      // Remove post-write delay
      post_write_delay.store(false);
    }

    bool needDump() {
      // If a writer is waiting or post-write delay is active, return NULL
      if (writer_waiting.load() || post_write_delay.load())
        return false;

      // Acquire shared lock for reading
      boost::shared_lock <boost::shared_mutex> lock(rw_mutex);

      // Re-check after acquiring lock (for safety)
      if (writer_waiting.load() || post_write_delay.load())
        return false;

      return entries.size() >= log_cap;
    }

public:
    DataCollectionServiceThread(std::string path, int cap, int delay)
            : log_path(path),
              log_cap(cap), dumpDelay(delay) {}

    void newEntry(DataEntry &entry) {
      // If a writer is waiting or post-write delay is active, return
      if (writer_waiting.load() || post_write_delay.load())
        return;

      // Acquire lock for adding entries
      boost::shared_lock<boost::shared_mutex> lock(rw_mutex);

      // Re-check after acquiring lock (for safety)
      if (writer_waiting.load() || post_write_delay.load())
        return;

      // Upgrade to a unique lock only when necessary (single writer allowed)
      boost::upgrade_lock<boost::shared_mutex> upgradeLock(rw_mutex);
      boost::upgrade_to_unique_lock<boost::shared_mutex> writeLock(upgradeLock);

      DataEntry newEntry = entry;
      entries.push_back(newEntry);
    }

    void *entry() override {
      int sleep_time = 1000000;
      while (1) {
        if (shutdown_thread.load()) {
          dump();
          break;
        }
        if (needDump()) {
          dump();
        }
        usleep(sleep_time);
      }
      return nullptr;
    }

    void shutdown() {
      shutdown_thread.store(true);
    }
};


#endif //CEPH_DATACOLLECTIONSERVICE_H
