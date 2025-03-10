//
// Created by Esmaeil on 3/9/25.
//

#ifndef CEPH_DATACOLLECTIONSOCKETHOOK_H
#define CEPH_DATACOLLECTIONSOCKETHOOK_H

#include "common/admin_socket.h"
#include "common/ceph_context.h"


class OSD;

class DataSocketHook : public AdminSocketHook
{
    CephContext *cct;
    OSD *osd;

public:
    static DataSocketHook *create(CephContext *cct_, OSD *osd);
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
             bufferlist &out) override;
};

#endif //CEPH_DATACOLLECTIONSOCKETHOOK_H
