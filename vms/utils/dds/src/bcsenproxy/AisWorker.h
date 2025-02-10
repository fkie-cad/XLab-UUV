#ifndef AISWORKER_H
#define AISWORKER_H

#include <ace/Mutex.h>
#include <tao/Basic_Types.h>

#include <string>

#include "PhysicalStateTypeSupportC.h"

struct AisWorkerArgs {
  PhysicalState::AivdmMessageDataWriter_var aivdm_dw;
  int rcv_port;
};
void* ais_worker(void*);

#endif
