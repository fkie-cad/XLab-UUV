#ifndef SENWORKER_H
#define SENWORKER_H

#include <ace/Mutex.h>
#include <tao/Basic_Types.h>

#include <string>

#include "PhysicalStateTypeSupportC.h"

struct SenWorkerArgs {
  PhysicalState::SensorsDataWriter_var sensors_dw;
  int rcv_port;
};
void* sen_worker(void*);

#endif
