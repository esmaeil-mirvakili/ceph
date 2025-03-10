//
// Created by Esmaeil on 3/9/25.
//

#ifndef CEPH_DATACOLLECTIONSOCKETHOOK_H
#define CEPH_DATACOLLECTIONSOCKETHOOK_H

#include "common/admin_socket.h"
#include "common/ceph_context.h"
#include "osd/OSD.h"

class DataSocketHook : public AdminSocketHook
{
    CephContext *cct;
    OSD *osd;

public:
    static DataSocketHook *create(CephContext *cct_, OSD *osd)
    {
      DataSocketHook *hook = nullptr;
      AdminSocket *admin_socket = cct_->get_admin_socket();
      if (admin_socket)
      {
        hook = new DataSocketHook(cct_, osd);
        int r = admin_socket->register_command("start data collection",
                                               hook,
                                               "start collecting data");
        r = admin_socket->register_command("stop data collection",
                                           hook,
                                           "stop and reset data collection");
        if (r != 0)
        {
          delete hook;
          hook = nullptr;
        }
      }
      return hook;
    }
    ~DataSocketHook()
    {
      AdminSocket *admin_socket = cct->get_admin_socket();
      admin_socket->unregister_commands(this);
    }

private:
    DataSocketHook(CephContext *cct_, OSD *osd) : cct(cct_), osd(osd) {}
    int call(std::string_view command, const cmdmap_t &cmdmap,
             const bufferlist &in,
             Formatter *f,
             std::ostream &ss,
             bufferlist &out) override
    {
      if (command == "start data collection")
      {
        osd->dataCollectionService.start();
      } else if (command == "stop data collection") {
        osd->dataCollectionService.stop();
        osd->dataCollectionService.dump();
      }
      return 0;
    }
};

#endif //CEPH_DATACOLLECTIONSOCKETHOOK_H
