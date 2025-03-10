#include "DataCollectionSocketHook.h"
#include "osd/OSD.h"

DataSocketHook *DataSocketHook::create(CephContext *cct_, OSD *osd) {
  DataSocketHook *hook = nullptr;
  AdminSocket *admin_socket = cct_->get_admin_socket();
  if (admin_socket) {
    hook = new DataSocketHook(cct_, osd);
    int r = admin_socket->register_command("start data collection",
                                           hook,
                                           "start collecting data");
    r = admin_socket->register_command("stop data collection",
                                       hook,
                                       "stop and reset data collection");
    if (r != 0) {
      delete hook;
      hook = nullptr;
    }
  }
  return hook;
}

int DataSocketHook::call(std::string_view command, const cmdmap_t &cmdmap,
                         const bufferlist &in,
                         Formatter *f,
                         std::ostream &ss,
                         bufferlist &out) {
  if (command == "start data collection") {
    osd->dataCollectionService.start();
  } else if (command == "stop data collection") {
    osd->dataCollectionService.stop();
    osd->dataCollectionService.dump();
  }
  return 0;
}