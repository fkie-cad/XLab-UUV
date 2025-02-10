#ifndef MISSION_CONTROLLER_H
#define MISSION_CONTROLLER_H

#include <ace/OS_NS_time.h>
#include "AutopilotC.h"
#include "AutopilotController.h"
class MissionController {
 public:
  explicit MissionController(AutopilotController *);
  void set_mission(Autopilot::Mission);
  void execute_command(Autopilot::MissionCommandType);
  CORBA::Boolean run();
  CORBA::Boolean is_report_available();
  Autopilot::MissionReport get_report();

 private:
  void execute_item(Autopilot::MissionItem);
  const ACE_hrtime_t REPORT_INTERVAL = 15 * 1e9;
  AutopilotController *ap_controller_;
  Autopilot::MissionStatus status_;
  Autopilot::Mission mission_;
  CORBA::Boolean mission_set_;
  CORBA::ULong mission_index_;
  Autopilot::MissionItemSeq mission_items_;
  ACE_hrtime_t item_start_ts_;
  ACE_hrtime_t item_timeout_;
  ACE_hrtime_t pause_start_ts_;
  ACE_hrtime_t last_report_ts_;
  CORBA::Boolean report_available_;
  Autopilot::MissionReport report_;
};

#endif
