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

#include "common/ceph_mutex.h"
#include "common/Thread.h"
#include "common/Clock.h"

#include <fstream>
#include <string>
#include <iostream>

struct DataCollectionRequestInfo {
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

    DataCollectionRequestInfo &operator=(const DataCollectionRequestInfo &other) {
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

    void print(std::ofstream &ss) const {
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
    }
};

struct DataCollectionOpInfo {
    uint32_t type;
    uint32_t cid;
    uint32_t oid;
    uint64_t off;
    uint64_t len;

    DataCollectionOpInfo(uint32_t _type, uint32_t _cid, uint32_t _oid, uint64_t _off,
                         uint64_t _len) : type(_type), cid(_cid), oid(_oid), off(_off),
                                          len(_len) {}

    std::string getHeader() {
      return "type, cid, oid, off, len";
    }

    void print(std::ofstream &ss) const {
      ss << type << ", ";
      ss << cid << ", ";
      ss << oid << ", ";
      ss << off << ", ";
      ss << len;
    }

};

class DataEntry {
public:
    std::string id;
    DataCollectionRequestInfo reqInfo;
    std::vector <DataCollectionOpInfo> ops;

    void log(std::ofstream &entryStream, std::ofstream &opsStream) {
      entryStream << id;
      reqInfo.print(entryStream);
      entryStream << std::endl;
      for (DataCollectionOpInfo &opInfo: ops) {
        opsStream << id;
        opInfo.print(opsStream);
        opsStream << std::endl;
      }
    }

public:
    DataEntry() {
      boost::uuids::uuid u = boost::uuids::random_generator()();
      id = boost::uuids::to_string(u);
    }

    DataCollectionRequestInfo &getReqInfo() {
      return reqInfo;
    }

    void addOp(uint32_t type, uint32_t cid, uint32_t oid, uint64_t off,
               uint64_t len) {
      ops.push_back(DataCollectionOpInfo(type, cid, oid, off, len));
    }

    DataEntry &operator=(const DataEntry &other) {
      if (this != &other) {
        id = other.id;
        reqInfo = other.reqInfo;
        ops.clear();
        for (auto &op: other.ops) {
          addOp(op.type, op.cid, op.oid, op.off, op.len);
        }
      }
      return *this;
    }

    friend class DataCollectionService;
};

class DataCollectionService{
protected:
    std::string log_path;
    std::vector <DataEntry> entries;

    void logEntries() {
      boost::uuids::uuid u = boost::uuids::random_generator()();
      std::string uid = boost::uuids::to_string(u);
      std::ofstream entryFile(log_path + "entries_" + uid + ".csv");
      std::ofstream opsFile(log_path + "ops_" + uid + ".csv");

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
public:
    bool active = false;
    void dump() {
      logEntries();
      entries.clear();
    }

    DataCollectionService(std::string path)
            : log_path(path) {}

    void newEntry(DataEntry &entry) {
      if(!active)
        return;
      DataEntry newEntry = entry;
      entries.push_back(newEntry);
    }
};


#endif //CEPH_DATACOLLECTIONSERVICE_H
