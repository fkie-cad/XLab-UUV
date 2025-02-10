#ifndef AUTOPILOT_CONTROLLER_H
#define AUTOPILOT_CONTROLLER_H

#include <ace/OS_NS_time.h>
#include <tao/Basic_Types.h>
#include <tao/DoubleSeqC.h>

#include <map>
#include <unordered_map>
#include <vector>

#include "AutopilotC.h"
#include "PhysicalStateC.h"
#include "PidController.h"

struct AisTarget {
  ACE_hrtime_t fix_ts_;
  PhysicalState::NavigationStatus fix_status_;
  CORBA::Long mmsi;
  double fix_lat_;
  double fix_lon_;
  double fix_rot_;  // rot is not available in BC
  double fix_cog_;  // in degrees
  double fix_sog_;  // in meters/s
};

class AutopilotController {
 public:
  AutopilotController();
  ~AutopilotController() = default;

  CORBA::Boolean update_state(Autopilot::AutopilotCommandType, CORBA::Boolean);
  CORBA::Boolean update_aivdm(
      const std::vector<PhysicalState::AivdmMessage>&);
  CORBA::Boolean set_route(Autopilot::Route);
  CORBA::Boolean set_loiter_position(Autopilot::LoiterPosition);
  CORBA::Boolean set_dive_procedure(Autopilot::DiveProcedure);
  CORBA::Boolean set_sensor_vals(PhysicalState::Sensors);
  CORBA::Boolean activate_procedure(Autopilot::ProcedureActivation,
                                    CORBA::Boolean);
  CORBA::Boolean execute();
  PhysicalState::Actuators get_actuator_cmds();
  CORBA::Boolean is_action_completed();

  CORBA::Boolean is_report_available();
  CORBA::Boolean is_colreg_report_available();
  Autopilot::APReport get_report();
  Autopilot::ColregStatus get_colreg_report();

 private:
  void reset_current_waypoint();
  CORBA::Boolean execute_loiter();
  CORBA::Boolean execute_route();
  CORBA::Boolean execute_stop();
  CORBA::Boolean execute_dive();
  CORBA::Boolean execute_maintain_depth();

  void set_state(Autopilot::AutopilotState);

  CORBA::Boolean activate_route(CORBA::Long);
  CORBA::Boolean activate_dive_procedure(CORBA::Long);
  CORBA::Boolean activate_loiter_position(CORBA::Long);

  CORBA::Double rudder_towards(Autopilot::Coordinates pos,
                               Autopilot::Coordinates wpt);
  CORBA::Double compute_throttle(CORBA::Double sog_setpoint);

  Autopilot::Coordinates get_position();
  void estimate_position(PhysicalState::Sensors);

  CORBA::Boolean execute_colreg(Autopilot::Coordinates&, CORBA::Double&);

  Autopilot::AutopilotState previous_state_;

  // report once every 750  milliseconds
  const ACE_hrtime_t REPORT_INTERVAL = 0.75 * 1e9;
  CORBA::Boolean report_available_;
  ACE_hrtime_t last_report_ts_;
  Autopilot::APReport report_;

  CORBA::Boolean action_completed_;

  std::unordered_map<CORBA::Long, Autopilot::Route> routes_;
  std::unordered_map<CORBA::Long, Autopilot::DiveProcedure> dive_procedures_;
  std::unordered_map<CORBA::Long, Autopilot::LoiterPosition> loiter_positions_;

  Autopilot::LoiterPosition active_loiter_position_;
  Autopilot::DiveProcedure active_dive_procedure_;
  Autopilot::Route active_route_;

  CORBA::Boolean route_is_set_;
  CORBA::Boolean loiter_position_is_set_;
  CORBA::Boolean dive_procedure_is_set_;

  CORBA::Boolean output_available_;
  CORBA::Boolean sensor_vals_set_;

  Autopilot::AutopilotState current_state_;

  Autopilot::WaypointSeq waypoint_seq_;
  CORBA::Long route_id_;
  Autopilot::Waypoint current_waypoint_;
  CORBA::DoubleSeq leg_len_seq_;
  CORBA::ULong current_waypoint_index_;
  CORBA::Long current_leg_len_;  // Unused for now, would be to adjust throttle

  PhysicalState::Actuators actuator_cmds_;
  PhysicalState::Sensors sensor_vals_;

  CORBA::Boolean reset_actuators_ = false;
  // steering constants
  // arrival radiuses should not be too small
  // or the boat might get stuck in a circle
  const CORBA::Double WPT_ARRIVAL_RADIUS = 35.0;
  const CORBA::Double LOITER_ARRIVAL_RADIUS = 35.0;
  const CORBA::Double LOITER_STAY_RADIUS =
      45.0;  // must be > LOITER_ARRIVAL_RADIUS
  const CORBA::Double DAMPENING_ROT = 6.0;

  const CORBA::Double DEPTH_TOLERANCE = 3.0;  // maximal acceptable depth error
  const ACE_hrtime_t DEPTH_TOLERANCE_TIMEOUT = 20 * 1e9;
  const CORBA::Double MIN_DEPTH_OFFSET = 2.5;  // must be > 1.0 (BC dpt limit)

  // COLREG data
  const ACE_hrtime_t COLREG_REPORT_INTERVAL = 1.45 * 1e9;
  const ACE_hrtime_t COLREG_UTURN_SAFEGUARD = 5 * 1e9;
  const CORBA::Double COLREG_CHECK_RADIUS = 750.0;
  // CPA distance under which a target is considered to be dangerous
  const CORBA::Double COLREG_CPAD = 57.0;
  ACE_hrtime_t last_colreg_rep_ts_ = 0;
  ACE_hrtime_t last_colreg_override_ = 0;
  CORBA::Double last_colreg_bearing_ = 0.0;

  std::map<CORBA::Long, AisTarget> ais_targets_{};
  CORBA::Boolean colreg_report_available_ = false;
  Autopilot::ColregStatus colreg_report_{};


  // PID controllers
  PidController engine_throttle_pid_{0.15, 0.05, 0.0, -1.0, 1.0};
  AngularPidController bow_thruster_pid_{0.0115, 0.00008, 0.00005, -0.7, 0.7};
  AngularPidController stern_thruster_pid_{-0.0115, -0.00008, -0.00005, -0.7,
                                           0.7};
  PidController ballast_tank_pid_{0.021, 0.00003, 0.001, -1.0, 1.0};

  CORBA::Double last_bearing = 0.0;
  CORBA::Double spins = 0.0;

  CORBA::Double sog_max_;

  CORBA::Double tgt_depth_ = 0.0;
  CORBA::Double tgt_depth_adjusted_ = 0.0;
  ACE_hrtime_t last_outside_depth_interval_ts = 0;

  Autopilot::Coordinates estimated_position_{0.0, 0.0};
  Autopilot::Coordinates previous_estimated_position_{0.0, 0.0};
  ACE_hrtime_t last_pos_estimate_ts_ = 0;

  CORBA::Boolean loiter_reached = false;
};

#endif
