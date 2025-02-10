#include "AutopilotController.h"

#include <ace/Log_Msg.h>
#include <ace/OS_NS_time.h>
#include <ace/ace_wchar.h>
#include <tao/Basic_Types.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>

#include "AutopilotC.h"
#include "Marmaths.h"

AutopilotController::AutopilotController() {
  this->routes_ = std::unordered_map<CORBA::Long, Autopilot::Route>();
  this->dive_procedures_ =
      std::unordered_map<CORBA::Long, Autopilot::DiveProcedure>();
  this->loiter_positions_ =
      std::unordered_map<CORBA::Long, Autopilot::LoiterPosition>();

  this->action_completed_ = false;
  this->route_is_set_ = false;
  this->loiter_position_is_set_ = false;
  this->dive_procedure_is_set_ = false;

  this->output_available_ = false;
  this->sensor_vals_set_ = false;

  this->current_waypoint_index_ = 0;

  this->current_state_ = Autopilot::AS_DISABLED;

  this->report_ = Autopilot::APReport();
  this->report_available_ = false;

  this->actuator_cmds_ = PhysicalState::Actuators();

  // if not overriden by route, never exceed 5m/s
  this->sog_max_ = 5.0;
}

CORBA::Boolean AutopilotController::is_report_available() {
  return this->report_available_;
}

CORBA::Boolean AutopilotController::is_colreg_report_available() {
  return this->colreg_report_available_;
}

Autopilot::APReport AutopilotController::get_report() {
  this->report_.state = this->current_state_;

  if (this->route_is_set_) {
    this->report_.active_route_id = this->route_id_;
    this->report_.tgt_speed = this->sog_max_;
    this->report_.route_progress = this->current_waypoint_index_ + 1;
    this->report_.route_len = this->waypoint_seq_.length();
    this->report_.route_name = this->routes_[this->route_id_].name;
    this->report_.active_waypoint = this->current_waypoint_;
  }

  Autopilot::Coordinates pos;
  if (this->sensor_vals_set_) {
    pos = this->get_position();
  }

  this->report_.gnss_ap = pos;

  if (this->loiter_position_is_set_) {
    this->report_.active_lp_id = this->active_loiter_position_.id;
    this->report_.lp_name = this->active_loiter_position_.position.name;
    if (this->sensor_vals_set_) {
      Autopilot::Coordinates dest =
          this->active_loiter_position_.position.coords;
      this->report_.lp_dist = distance_harvesine(pos.latitude, pos.longitude,
                                                 dest.latitude, dest.longitude);
    }
  }

  if (this->dive_procedure_is_set_) {
    this->report_.dp_name = this->active_dive_procedure_.name;
    this->report_.tgt_depth = this->active_dive_procedure_.depth;
  }

  this->last_report_ts_ = ACE_OS::gethrtime();
  this->report_available_ = false;
  return this->report_;
}

Autopilot::ColregStatus AutopilotController::get_colreg_report() {
  this->colreg_report_available_ = false;
  return this->colreg_report_;
}

CORBA::Boolean AutopilotController::is_action_completed() {
  if (this->action_completed_) {
    this->action_completed_ = false;
    return true;
  }
  return false;
}

CORBA::Boolean AutopilotController::activate_procedure(
    Autopilot::ProcedureActivation proc_act, CORBA::Boolean log_completion) {
  if (log_completion) {
    this->action_completed_ = true;
  }

  switch (proc_act.procedure) {
    case Autopilot::PROC_ROUTE: {
      return this->activate_route(proc_act.procedure_id);
      break;
    }
    case Autopilot::PROC_LOITERPOSITION: {
      return this->activate_loiter_position(proc_act.procedure_id);
      break;
    }
    case Autopilot::PROC_DIVEPROCEDURE: {
      return this->activate_dive_procedure(proc_act.procedure_id);
    }
  }
  this->report_available_ = true;
  return false;
}

CORBA::Boolean AutopilotController::activate_route(CORBA::Long route_id) {
  if (this->routes_.find(route_id) == this->routes_.end()) return true;

  Autopilot::Route new_route = this->routes_[route_id];
  Autopilot::WaypointSeq waypoint_seq = new_route.waypoints;

  this->waypoint_seq_ = waypoint_seq;
  this->sog_max_ = new_route.planned_speed * KNT_TO_MS;

  if (this->route_is_set_ && this->route_id_ == new_route.id) {
    // we're re-activating the currently active route, has it changed?
    // check if active waypoint is still in bounds, otherwise set last waypoint
    // as active
    if (this->current_waypoint_index_ >= waypoint_seq_.length()) {
      this->current_waypoint_index_ = waypoint_seq_.length() - 1;
    }
    this->current_waypoint_ =
        this->waypoint_seq_[this->current_waypoint_index_];
  } else {
    // we're changing route, assume we want to follow it from the start
    this->route_is_set_ = true;
    this->reset_current_waypoint();
  }

  this->route_is_set_ = true;
  this->route_id_ = new_route.id;

  // pre-compute the length of all legs
  this->leg_len_seq_.length(waypoint_seq.length());
  for (uint i=0; i < waypoint_seq.length() - 1; ++i) {
    Autopilot::Coordinates from = waypoint_seq[i].coords;
    Autopilot::Coordinates to = waypoint_seq[i + 1].coords;
    this->leg_len_seq_[i] = distance_harvesine(from.latitude, from.longitude,
                                               to.latitude, to.longitude);
  }

  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("Activated route:\n"
                      "    route name:      %s\n"
                      "    route length:    %i\n"
                      "    active waypoint: %s\n"
                      "    waypoint index:  %i\n"),
             (const char*)new_route.name, waypoint_seq.length(),
             (const char*)this->current_waypoint_.name,
             this->current_waypoint_index_));

  return false;
}

CORBA::Boolean AutopilotController::activate_loiter_position(
    CORBA::Long loiter_position_id) {
  if (this->loiter_positions_.find(loiter_position_id) ==
      this->loiter_positions_.end())
    return true;

  this->active_loiter_position_ = this->loiter_positions_[loiter_position_id];
  this->loiter_position_is_set_ = true;
  return false;
}

CORBA::Boolean AutopilotController::activate_dive_procedure(
    CORBA::Long dive_procedure_id) {
  if (this->dive_procedures_.find(dive_procedure_id) ==
      this->dive_procedures_.end())
    return true;

  this->active_dive_procedure_ = this->dive_procedures_[dive_procedure_id];
  this->dive_procedure_is_set_ = true;
  return false;
}

void AutopilotController::set_state(Autopilot::AutopilotState new_state) {
  this->previous_state_ = this->current_state_;
  this->current_state_ = new_state;
  // pre-emptively reset actuators on any state transition:
  // if we're transitioning to an active state, it will overwrite
  // actuators, if we're disabling the AP, we want to kill the engine
  // and fall back to manual control
  this->reset_actuators_ = true;
}

CORBA::Boolean AutopilotController::update_state(
    Autopilot::AutopilotCommandType command, CORBA::Boolean log_completion) {
  if (!log_completion) {
    // state change called directly by CCC, if MissionController was running,
    // it has been suspended, mark the current action as completed so that it
    // is skipped if the mission is resumed
    this->action_completed_ = true;
  } else {
    this->action_completed_ = false;
  }

  switch (this->current_state_) {
    case Autopilot::AS_DISABLED: {
      switch (command) {
        case Autopilot::AC_ROUTE_START: {
          if (this->route_is_set_) this->set_state(Autopilot::AS_ROUTE_ENABLED);
          break;
        }
        case Autopilot::AC_LOITER_START: {
          if (this->loiter_position_is_set_)
            this->set_state(Autopilot::AS_LOITERING);
          break;
        }
        case Autopilot::AC_DIVE_START: {
          if (this->dive_procedure_is_set_)
            this->set_state(Autopilot::AS_DIVING);
          break;
        }
        case Autopilot::AC_EMERGENCY_STOP: {
          this->set_state(Autopilot::AS_EMERGENCY_STOP);
          break;
        }
        default: {
          // stay disabled
          if (log_completion) this->action_completed_ = true;
        }
      }
      break;
    }

    case Autopilot::AS_ROUTE_ENABLED: {
      switch (command) {
        case Autopilot::AC_ROUTE_STOP: {
          this->reset_current_waypoint();
          this->set_state(Autopilot::AS_DISABLED);
          if (log_completion) this->action_completed_ = true;
          break;
        }
        case Autopilot::AC_ROUTE_SUSPEND: {
          this->set_state(Autopilot::AS_ROUTE_SUSPENDED);
          if (log_completion) this->action_completed_ = true;
          break;
        }
        case Autopilot::AC_LOITER_START: {
          if (this->loiter_position_is_set_)
            this->set_state(Autopilot::AS_LOITERING);
          break;
        }
        case Autopilot::AC_DIVE_START: {
          if (this->dive_procedure_is_set_)
            this->set_state(Autopilot::AS_DIVING);
          break;
        }
        case Autopilot::AC_EMERGENCY_STOP: {
          this->set_state(Autopilot::AS_EMERGENCY_STOP);
          break;
        }
        default: {
          // stay enabled
        }
      }
      break;
    }

    case Autopilot::AS_ROUTE_SUSPENDED: {
      switch (command) {
        case Autopilot::AC_ROUTE_RESUME: {
          this->set_state(Autopilot::AS_ROUTE_ENABLED);
          break;
        }
        case Autopilot::AC_ROUTE_STOP: {
          this->reset_current_waypoint();
          this->set_state(Autopilot::AS_DISABLED);
          if (log_completion) this->action_completed_ = true;
          break;
        }
        case Autopilot::AC_LOITER_START: {
          if (this->loiter_position_is_set_)
            this->set_state(Autopilot::AS_LOITERING);
          break;
        }
        case Autopilot::AC_DIVE_START: {
          if (this->dive_procedure_is_set_)
            this->set_state(Autopilot::AS_DIVING);
          break;
        }
        case Autopilot::AC_EMERGENCY_STOP: {
          this->set_state(Autopilot::AS_EMERGENCY_STOP);
          break;
        }
        default: {
          // stay suspended
          if (log_completion) this->action_completed_ = true;
        }
      }
      break;
    }

    case Autopilot::AS_LOITERING: {
      switch (command) {
        case Autopilot::AC_LOITER_STOP: {
          this->loiter_reached = false;
          if (this->previous_state_ == Autopilot::AS_ROUTE_SUSPENDED) {
            this->set_state(Autopilot::AS_ROUTE_SUSPENDED);
          } else {
            this->set_state(Autopilot::AS_DISABLED);
          }
          if (log_completion) this->action_completed_ = true;
          break;
        }
        case Autopilot::AC_EMERGENCY_STOP: {
          this->loiter_reached = false;
          this->set_state(Autopilot::AS_EMERGENCY_STOP);
          break;
        }
        default: {
          // only way to stop loitering is explicit stop or emergency stop
        }
      }
      break;
    }

    case Autopilot::AS_EMERGENCY_STOP: {
      switch (command) {
        case Autopilot::AC_ROUTE_STOP: {
          // interpret ROUTE_STOP as forcing to rescind control
          this->reset_current_waypoint();
          this->set_state(Autopilot::AS_DISABLED);
          if (log_completion) this->action_completed_ = true;
          break;
        }
        default: {
          // no other manual transition out of emergency stop state
        }
      }
      break;
    }

    case Autopilot::AS_DIVING: {
      // TODO(Anyone)
      switch (command) {
        case Autopilot::AC_DIVE_STOP: {
          if (this->route_is_set_) {
            this->set_state(Autopilot::AS_ROUTE_SUSPENDED);
          } else {
            this->set_state(Autopilot::AS_DISABLED);
          }
          break;
        }
        case Autopilot::AC_EMERGENCY_STOP: {
          this->loiter_reached = false;
          this->set_state(Autopilot::AS_EMERGENCY_STOP);
          break;
        }
        default: {
          // only explicitely stopping or emergency stop gets out of diving
        }
      }
      // if (log_completion) this->action_completed_ = true;
      break;
    }
    case Autopilot::AS_UNKNOWN: {
      // should never happen, silently drop
      break;
    }
  }

  this->report_available_ = true;
  // error on illegal state transition instead?
  return false;
}

void AutopilotController::reset_current_waypoint() {
  this->current_waypoint_index_ = 0;
  if (this->route_is_set_) {
    this->current_waypoint_ =
        this->waypoint_seq_[this->current_waypoint_index_];
  }
}

CORBA::Boolean AutopilotController::set_route(Autopilot::Route new_route) {
  // receive route data, store it but don't set as active yet
  Autopilot::WaypointSeq waypoint_seq = new_route.waypoints;
  if (waypoint_seq.length() == 0) {
    // reject route
    // TODO(Anyone): Inform CCC
    return true;
  }

  this->routes_[new_route.id] = new_route;
  // trigger waypoint update if this route is already active
  if (this->route_is_set_ && this->route_id_ == new_route.id) {
    this->activate_route(new_route.id);
  }

  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("Stored route:\n"
                      "    route id:        %i\n"
                      "    route name:      %s\n"
                      "    route length:    %i\n"),
             new_route.id, (const char*)new_route.name, waypoint_seq.length()));

  return false;
}

CORBA::Boolean AutopilotController::set_loiter_position(
    Autopilot::LoiterPosition loiter_position) {
  this->loiter_positions_[loiter_position.id] = loiter_position;
  if (this->loiter_position_is_set_ &&
      this->active_loiter_position_.id == loiter_position.id) {
    this->active_loiter_position_ = this->loiter_positions_[loiter_position.id];
  }
  return false;
}

CORBA::Boolean AutopilotController::set_dive_procedure(
    Autopilot::DiveProcedure dive_procedure) {
  this->dive_procedures_[dive_procedure.id] = dive_procedure;
  if (this->dive_procedure_is_set_ &&
      this->active_dive_procedure_.id == dive_procedure.id) {
    this->active_dive_procedure_ = this->dive_procedures_[dive_procedure.id];
  }
  return false;
}

CORBA::Boolean AutopilotController::set_sensor_vals(
    PhysicalState::Sensors sensors) {
  this->estimate_position(sensors);
  this->sensor_vals_set_ = true;
  this->sensor_vals_ = sensors;
  this->actuator_cmds_.bc_id = this->sensor_vals_.bc_id;
  return false;
}

CORBA::Boolean AutopilotController::update_aivdm(
    const std::vector<PhysicalState::AivdmMessage>& targets) {
  if (!this->sensor_vals_set_) {
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("No AIVDM message received.")));
    return false;
  }

  ACE_hrtime_t now = ACE_OS::gethrtime();
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("AIVDM message(s) received:\n")));
  for (auto target : targets) {
    if (this->ais_targets_.find(target.mmsi) == this->ais_targets_.end()) {
      this->ais_targets_[target.mmsi] = AisTarget{now,
                                                  target.navigation_status,
                                                  target.mmsi,
                                                  target.latitude,
                                                  target.longitude,
                                                  target.rate_of_turn,
                                                  target.course_over_ground,
                                                  target.speed_over_ground};
    } else {
      this->ais_targets_[target.mmsi].fix_ts_ = now,
      this->ais_targets_[target.mmsi].fix_status_ = target.navigation_status;
      this->ais_targets_[target.mmsi].mmsi = target.mmsi,
      this->ais_targets_[target.mmsi].fix_lat_ = target.latitude;
      this->ais_targets_[target.mmsi].fix_lon_ = target.longitude;
      this->ais_targets_[target.mmsi].fix_rot_ = target.rate_of_turn;
      this->ais_targets_[target.mmsi].fix_cog_ = target.course_over_ground;
      this->ais_targets_[target.mmsi].fix_sog_ = target.speed_over_ground;
    }
    ACE_DEBUG((LM_DEBUG, 
              ACE_TEXT(
                "New AIVDM message:\n"
                "\tTS:\t\t%f\n"
                "\tStatus:\t\t%i\n"
                "\tMMSI:\t\t%i\n"
                "\tLAT:\t\t%f\n"
                "\tLON:\t\t%f\n"
                "\tCOG:\t\t%f\n"
                "\tSOG:\t\t%f\n\n"
              ),
              now,
              target.navigation_status,
              target.mmsi,
              target.latitude,
              target.longitude,
              target.course_over_ground,
              target.speed_over_ground
              ));
  }
  return false;
}

void AutopilotController::estimate_position(PhysicalState::Sensors sensors) {
  ACE_hrtime_t now = ACE_OS::gethrtime();

  // Sensors values we fuse. Duplication of gnss_2 and gnss_1 are placeholders
  // in case we can't do dead reckoning or constant movement bias yet due to
  // lack of previous estimates
  std::array<CORBA::Double, 5> lat_vals = {
      sensors.gnss_1.latitude, sensors.gnss_2.latitude, sensors.gnss_3.latitude,
      sensors.gnss_2.latitude, sensors.gnss_1.latitude};
  std::array<CORBA::Double, 5> lon_vals = {
      sensors.gnss_1.longitude, sensors.gnss_2.longitude,
      sensors.gnss_3.longitude, sensors.gnss_2.longitude,
      sensors.gnss_1.longitude};

  if (this->sensor_vals_set_) {
    // Dead reckoning, guess cog and sog by averaging previous and current value
    // Compute time delta in seconds to extrapolate new position from sog
    CORBA::Double delta = (now - this->last_pos_estimate_ts_) * 1e-9;
    CORBA::Double sog =
        (this->sensor_vals_.speed_over_ground + sensors.speed_over_ground) / 2;
    CORBA::Double cog =
        (this->sensor_vals_.course_over_ground + sensors.course_over_ground) /
        2;

    CORBA::Double lat_shift;
    CORBA::Double lon_shift;
    std::tie(lat_shift, lon_shift) = polar_to_cartesian(cog, sog * delta);
    lat_vals[3] = shift_lat(this->estimated_position_.latitude,
                            this->estimated_position_.longitude, lat_shift);
    lon_vals[3] = shift_long(this->estimated_position_.longitude,
                             this->estimated_position_.longitude, lon_shift);

    if (this->previous_estimated_position_.latitude != 0 &&
        this->previous_estimated_position_.longitude != 0) {
      // add bias towards constant movement
      lat_vals[4] = this->estimated_position_.latitude +
                    (this->estimated_position_.latitude -
                     this->previous_estimated_position_.latitude);
      lon_vals[4] = this->estimated_position_.longitude +
                    (this->estimated_position_.longitude -
                     this->previous_estimated_position_.longitude);
    }
  }

  std::sort(lat_vals.begin(), lat_vals.end());
  std::sort(lon_vals.begin(), lon_vals.end());

  CORBA::Long lat_count = 0;
  CORBA::Long lon_count = 0;
  CORBA::Double lat_sum = 0.0;
  CORBA::Double lon_sum = 0.0;

  for (int i = 0; i < 5; ++i) {
    // discard sensor/estimate lat or lon val if it seems implausible (>10m
    // error), use the median value as reference as a sort of voting scheme
    CORBA::Double lat_offset =
        distance_harvesine(lat_vals[i], lon_vals[2], lat_vals[2], lon_vals[2]);
    CORBA::Double lon_offset =
        distance_harvesine(lat_vals[2], lon_vals[i], lat_vals[2], lon_vals[2]);
    if (std::abs(lat_offset) < 10.0) {
      lat_sum += lat_vals[i];
      lat_count++;
    }
    if (std::abs(lon_offset) < 10.0) {
      lon_sum += lon_vals[i];
      lon_count++;
    }
  }

  this->previous_estimated_position_ = this->estimated_position_;

  // compute mean values of plausible sensors to smooth out noise
  this->estimated_position_.latitude = lat_sum / lat_count;
  this->estimated_position_.longitude = lon_sum / lon_count;

  this->last_pos_estimate_ts_ = now;
}

CORBA::Boolean AutopilotController::execute() {
  ACE_hrtime_t now = ACE_OS::gethrtime();
  // have we sent a report recently?
  if (now - this->last_report_ts_ >= REPORT_INTERVAL) {
    this->report_available_ = true;
  }

  // do we write to actuators?
  this->output_available_ = false;

  if (this->reset_actuators_) {
    this->reset_actuators_ = false;
    // kill engine and thrusters and reset rudder
    this->actuator_cmds_.rudder_angle = 0.0;
    this->actuator_cmds_.engine_throttle_port = 0.0;
    this->actuator_cmds_.engine_throttle_stbd = 0.0;
    this->actuator_cmds_.thruster_throttle_bow = 0.0;
    this->actuator_cmds_.thruster_throttle_stern = 0.0;
    this->output_available_ = true;
  }

  switch (this->current_state_) {
    case Autopilot::AS_DISABLED: {
      // maintaining depth overrides the AP being "disabled"
      this->output_available_ |= this->execute_maintain_depth();
      break;
    }
    case Autopilot::AS_ROUTE_SUSPENDED: {
      this->output_available_ |= this->execute_maintain_depth();
      break;
    }
    case Autopilot::AS_ROUTE_ENABLED: {
      this->output_available_ |= this->execute_route();
      this->output_available_ |= this->execute_maintain_depth();
      break;
    }
    case Autopilot::AS_LOITERING: {
      this->output_available_ |= this->execute_loiter();
      this->output_available_ |= this->execute_maintain_depth();
      break;
    }
    case Autopilot::AS_EMERGENCY_STOP: {
      this->output_available_ |= this->execute_stop();
      this->output_available_ |= this->execute_maintain_depth();
      break;
    }
    case Autopilot::AS_DIVING: {
      this->output_available_ |= this->execute_dive();
      // no need to call maintain depth again, since it's done in execute_dive()
      // we need to do that there to work with the adjusted tgt depth when
      // deciding if we're done diving
      break;
    }
    case Autopilot::AS_UNKNOWN: {
      // should never happen, drop silently
      break;
    }
  }
  return this->output_available_;
}

CORBA::Boolean AutopilotController::execute_colreg(
    Autopilot::Coordinates& wpt_override, CORBA::Double& speed_override) {
  if (!this->sensor_vals_set_) return false;
  ACE_hrtime_t now = ACE_OS::gethrtime();

  if (now - this->last_colreg_rep_ts_ > COLREG_REPORT_INTERVAL) {
    // prepare a report
    this->last_colreg_rep_ts_ = now;
    this->colreg_report_available_ = true;
  }

  Autopilot::Coordinates own_pos = this->get_position();

  // own position relative to current position
  // in meters and ignoring earth curvature to reduce expensive trigonometry
  std::array<Autopilot::Coordinates, 65> future_own_pos{};
  CORBA::Double own_lat_shift;
  CORBA::Double own_lon_shift;
  std::tie(own_lat_shift, own_lon_shift) =
      polar_to_cartesian(this->sensor_vals_.course_over_ground,
                         this->sensor_vals_.speed_over_ground);
  for (uint i = 0; i < future_own_pos.size(); ++i) {
    future_own_pos[i] =
        Autopilot::Coordinates{own_lat_shift * i, own_lon_shift * i};
  }

  // info about the most pressing target
  CORBA::Double cpa_t_min = DBL_MAX;
  const AisTarget* tgt_info;
  CORBA::Double tgt_bearing;
  Autopilot::Coordinates tgt_estimated_pos{};

  ACE_DEBUG((LM_DEBUG, ACE_TEXT("COLREG check:\n")));

  // iterate over all tracked targets
  for (const auto& target : this->ais_targets_) {
    const AisTarget* current_tgt_info = &target.second;

    // time since fix in seconds
    CORBA::Double fix_delta = (now - current_tgt_info->fix_ts_) * 1e-9;

    // estimated movement each second
    CORBA::Double lat_shift;
    CORBA::Double lon_shift;
    std::tie(lat_shift, lon_shift) = polar_to_cartesian(
        current_tgt_info->fix_cog_, current_tgt_info->fix_sog_);

    // estimated current position
    CORBA::Double estimated_lat =
        shift_lat(current_tgt_info->fix_lat_, current_tgt_info->fix_lon_,
                  lat_shift * fix_delta);
    CORBA::Double estimated_lon =
        shift_long(current_tgt_info->fix_lat_, current_tgt_info->fix_lon_,
                   lon_shift * fix_delta);

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("    mmsi: %i"), current_tgt_info->mmsi));

    // determine current distance, skip targets which we can
    // (probably!) safely ignore for now
    CORBA::Double current_distance = distance_harvesine(
        own_pos.latitude, own_pos.longitude, estimated_lat, estimated_lon);
    if (current_distance > COLREG_CHECK_RADIUS) {
      ACE_DEBUG((LM_DEBUG, ACE_TEXT(" -> out of range (%f > %f)\n"),
                 current_distance, COLREG_CHECK_RADIUS));
      continue;
    }
    CORBA::Double current_bearing = relative_bearing(
        0.0,  // absolute bearing
        own_pos.latitude, own_pos.longitude, estimated_lat, estimated_lon);
    Autopilot::Coordinates relative_pos{};
    std::tie(relative_pos.latitude, relative_pos.longitude) =
        polar_to_cartesian(current_bearing, current_distance);

    // determine cpa
    CORBA::Double local_cpa_d = DBL_MAX;
    CORBA::Double current_d;
    CORBA::Double local_cpa_t;
    for (uint i = 0; i < future_own_pos.size(); ++i) {
      current_d = std::sqrt(
          std::pow(future_own_pos[i].latitude - relative_pos.latitude, 2) +
          std::pow(future_own_pos[i].longitude - relative_pos.longitude, 2));
      if (current_d < local_cpa_d) {
        local_cpa_d = current_d;
      } else {
        // we expect the first minimum found to be global, since
        // heading and speed of both vessels are constant
        break;
      }
      local_cpa_t = i;
      relative_pos.latitude += lat_shift;
      relative_pos.longitude += lon_shift;
    }

    ACE_DEBUG((LM_DEBUG, ACE_TEXT(" -> CPA distance: %f, CPA time: %f\n"),
               local_cpa_d, local_cpa_t));

    if (local_cpa_d < COLREG_CPAD && local_cpa_t - 1.0 < cpa_t_min) {
      // new candidate for most pressing target
      cpa_t_min = local_cpa_t;
      tgt_info = current_tgt_info;
      tgt_bearing = std::fmod(
          360.0 + current_bearing - this->sensor_vals_.course_over_ground,
          360.0);
      tgt_estimated_pos.latitude = estimated_lat;
      tgt_estimated_pos.longitude = estimated_lon;
    }
  }

  CORBA::Double wpt_bearing = relative_bearing(
      this->sensor_vals_.course_over_ground, own_pos.latitude,
      own_pos.longitude, wpt_override.latitude, wpt_override.longitude);
  if (wpt_bearing > 180.0) wpt_bearing -= 360.0;

  if (cpa_t_min != DBL_MAX) {
    // we have a COLREG situation, but which type?
    Autopilot::ColregType situation;

    CORBA::Double tgt_rel_heading = relative_heading(
        this->sensor_vals_.course_over_ground, tgt_info->fix_cog_);

    if (tgt_rel_heading > 180.0) tgt_rel_heading -= 360.0;
    if (tgt_bearing > 180.0) tgt_bearing -= 360.0;

    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("COLREG execution:\n"
                        "    mmsi:      %i\n"
                        "    t_cpa:     %f\n"
                        "    rel_h:     %f\n"
                        "    bearing:   %f\n"
                        "    tgt_pos:   %f, %f\n"),
               tgt_info->mmsi, cpa_t_min, tgt_rel_heading, tgt_bearing,
               tgt_estimated_pos.latitude, tgt_estimated_pos.longitude));

    if (std::abs(tgt_rel_heading) <= 22.5 && tgt_info->fix_sog_ > 0.1) {
      if (std::abs(tgt_bearing) < 45) {
        // overtaking a moving target, don't change wpt, reduce speed to be a
        // bit lower than that of the vessel we are overtaking
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("    situation: OVERTAKING\n")));
        situation = Autopilot::CR_OVERTAKING;
        speed_override = std::min(speed_override, tgt_info->fix_sog_ * 0.8);
      } else {
        // target is behind us, speed up to avoid getting overtaken
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("    situation: OVERTAKEN\n")));
        situation = Autopilot::CR_OVERTAKEN;
        speed_override = std::max(speed_override, tgt_info->fix_sog_ * 1.2);
      }
    } else if (std::abs(tgt_rel_heading) <= 157.5) {
      // crossing, change wpt to n meters behind tgt vessel, unless wpt is
      // further in the right direction
      ACE_DEBUG((LM_DEBUG, ACE_TEXT("    situation: CROSSING\n")));
      situation = Autopilot::CR_CROSSING;

      CORBA::Double colreg_lat_offset;
      CORBA::Double colreg_lon_offset;
      std::tie(colreg_lat_offset, colreg_lon_offset) = polar_to_cartesian(
          std::fmod(tgt_info->fix_cog_ + 180.0, 360.0), COLREG_CPAD * 1.6);
      CORBA::Double colreg_lat =
          shift_lat(tgt_estimated_pos.latitude, tgt_estimated_pos.longitude,
                    colreg_lat_offset);
      CORBA::Double colreg_lon =
          shift_long(tgt_estimated_pos.latitude, tgt_estimated_pos.longitude,
                     colreg_lon_offset);
      CORBA::Double colreg_bearing = relative_bearing(
          this->sensor_vals_.course_over_ground, own_pos.latitude,
          own_pos.longitude, colreg_lat, colreg_lon);

      if (colreg_bearing > 180.0) colreg_bearing -= 360.0;

      if (wpt_bearing * colreg_bearing >= 0 &&
          std::abs(wpt_bearing) > std::abs(colreg_bearing)) {
        // only slow down a bit, steering to wpt should be enough
        // to resolve collision threat
        speed_override = 0.85 * speed_override;
      } else {
        wpt_override.latitude = colreg_lat;
        wpt_override.longitude = colreg_lon;
        speed_override = 0.65 * speed_override;
        if (cpa_t_min < 10.0) {
          // last ditch effort, maybe braking helps
          speed_override = 0.3 * speed_override;
        }
      }
    } else {
      ACE_DEBUG((
          LM_DEBUG,
          ACE_TEXT("    situation: HEAD-TO-HEAD OR OVERTAKING SLOW TARGET\n")));
      // head to head or overtaking a stationary/slow target
      // change wpt to n meters perpendicular to and behind tgt vessel in
      // direction opposite to bearing, unless wpt is further in the right
      // direction
      situation = Autopilot::CR_HEAD_TO_HEAD;

      // dodge to rel bearing direction as seen from other tgt
      CORBA::Double inv_bearing = relative_bearing(
          tgt_info->fix_cog_, tgt_estimated_pos.latitude,
          tgt_estimated_pos.longitude, own_pos.latitude, own_pos.longitude);
      CORBA::Double direction = 1.0;
      if (inv_bearing > 180.0) direction = -1.0;

      CORBA::Double colreg_lat_offset;
      CORBA::Double colreg_lon_offset;
      std::tie(colreg_lat_offset, colreg_lon_offset) = polar_to_cartesian(
          std::fmod(360.0 + tgt_info->fix_cog_ + (direction * 156.5), 360.0),
          2.2 * COLREG_CPAD);
      CORBA::Double colreg_lat =
          shift_lat(tgt_estimated_pos.latitude, tgt_estimated_pos.longitude,
                    colreg_lat_offset);
      CORBA::Double colreg_lon =
          shift_long(tgt_estimated_pos.latitude, tgt_estimated_pos.longitude,
                     colreg_lon_offset);
      CORBA::Double colreg_bearing = relative_bearing(
          this->sensor_vals_.course_over_ground, own_pos.latitude,
          own_pos.longitude, colreg_lat, colreg_lon);

      if (colreg_bearing > 180.0) colreg_bearing -= 360.0;

      // wpt on the same side
      if (wpt_bearing * colreg_bearing >= 0
          // wpt further on that side
          && std::abs(wpt_bearing) > std::abs(colreg_bearing) &&
          // not a dangerous U-turn
          !(std::abs(tgt_bearing) > 90.0 && (std::abs(wpt_bearing) > 90.0))) {
        // only slow down a bit, steering to wpt should be enough
        // to resolve collision threat
        speed_override = 0.95 * speed_override;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("    action:    override speed\n")));
      } else {
        wpt_override.latitude = colreg_lat;
        wpt_override.longitude = colreg_lon;
        speed_override = 0.84 * speed_override;
        ACE_DEBUG((LM_DEBUG,
                   ACE_TEXT("    action:    override speed and steering\n")));
      }
    }
    this->colreg_report_.type = situation;
    this->colreg_report_.tgt_mmsi = tgt_info->mmsi;
    this->colreg_report_.tgt_pos.latitude = tgt_info->fix_lat_;
    this->colreg_report_.tgt_pos.longitude = tgt_info->fix_lon_;

    this->last_colreg_bearing_ = relative_bearing(
        0.0, own_pos.latitude, own_pos.longitude, wpt_override.latitude,
        wpt_override.longitude);  // store desired heading
    this->last_colreg_override_ = now;

    return true;
  } else {
    // No pressing target, but check if we should still be careful

    // AP wants a radical course chang and we recently performed COLREG
    if (std::abs(wpt_bearing) > 90.0 &&
        now - this->last_colreg_override_ < COLREG_UTURN_SAFEGUARD) {
      // keep heading in last COLREG override direction, let AP handle speed
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("COLREG execution: keep previous COLREG heading\n")));
      CORBA::Double colreg_lat_shift;
      CORBA::Double colreg_lon_shift;
      std::tie(colreg_lat_shift, colreg_lon_shift) = polar_to_cartesian(
          this->last_colreg_bearing_, (1 + speed_override) * 30);
      wpt_override.latitude =
          shift_lat(own_pos.latitude, own_pos.longitude, colreg_lat_shift);
      wpt_override.longitude =
          shift_long(own_pos.latitude, own_pos.longitude, colreg_lon_shift);
    }
    // don't perform colreg, inform CCC
    this->colreg_report_.type = Autopilot::CR_INACTIVE;
  }

  return false;
}

CORBA::Boolean AutopilotController::execute_route() {
  if (!this->sensor_vals_set_) return false;

  Autopilot::Coordinates pos = this->get_position();
  Autopilot::Waypoint wpt = this->current_waypoint_;

  CORBA::Double d_to_waypoint = distance_harvesine(
      pos.latitude, pos.longitude, wpt.coords.latitude, wpt.coords.longitude);

  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("Executing route towards:\n"
                      "    target waypoint: %s\n"
                      "    distance:        %f\n"),
             (const char*)wpt.name, d_to_waypoint));

  // check if active waypoint needs to be updated
  if (d_to_waypoint < WPT_ARRIVAL_RADIUS) {
    if (this->current_waypoint_index_ >= this->waypoint_seq_.length() - 1) {
      // reached last waypoint, stop autopilot
      // FIXME: Shouldn't we unset the route here?
      this->reset_current_waypoint();
      this->set_state(Autopilot::AS_DISABLED);
      this->action_completed_ = true;

      return false;
    }
    this->current_waypoint_index_++;
    this->current_waypoint_ =
        this->waypoint_seq_[this->current_waypoint_index_];
    wpt = this->current_waypoint_;
    d_to_waypoint = distance_harvesine(
        pos.latitude, pos.longitude, wpt.coords.latitude, wpt.coords.longitude);
  }

  // This could be improved by adding braking on leg end/tight turns etc.
  CORBA::Double requested_sog = 1.0 * this->sog_max_;
  Autopilot::Coordinates requested_wpt =
      Autopilot::Coordinates{wpt.coords.latitude, wpt.coords.longitude};

  // let COLREG procedure override speed and course
  this->execute_colreg(requested_wpt, requested_sog);

  // determine engine throttle,
  CORBA::Double throttle = compute_throttle(requested_sog);
  this->actuator_cmds_.engine_throttle_port = throttle;
  this->actuator_cmds_.engine_throttle_stbd = throttle;
  // always set thrusters to 0.0, we only use rudder to steer
  // during normal operation
  this->actuator_cmds_.thruster_throttle_bow = 0.0;
  this->actuator_cmds_.thruster_throttle_stern = 0.0;

  // steering using the rudder
  // to improve this, assisting with engine/thrusters could be added
  this->actuator_cmds_.rudder_angle = rudder_towards(pos, requested_wpt);

  return true;
}

CORBA::Boolean AutopilotController::execute_loiter() {
  if (!this->sensor_vals_set_) return false;

  Autopilot::Coordinates pos = this->get_position();
  Autopilot::Coordinates target_pos = Autopilot::Coordinates{
      this->active_loiter_position_.position.coords.latitude,
      this->active_loiter_position_.position.coords.longitude};
  CORBA::Double target_bearing = this->active_loiter_position_.bearing;
  CORBA::Double bearing = this->sensor_vals_.heading;

  CORBA::Double d_to_target = distance_harvesine(
      pos.latitude, pos.longitude, target_pos.latitude, target_pos.longitude);

  if (!loiter_reached && d_to_target < LOITER_ARRIVAL_RADIUS) {
    loiter_reached = true;
    this->action_completed_ = true;
  }
  if (loiter_reached && d_to_target > LOITER_STAY_RADIUS) {
    loiter_reached = false;
  }

  if (!loiter_reached) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("Executing loiter towards:\n"
                        "    target:   %f, %f\n"
                        "    distance: %f\n"),
               target_pos.latitude, target_pos.longitude, d_to_target));
    // route towards target like any a route waypoint, smoothly reducing speed
    // don't use thrusters
    this->actuator_cmds_.thruster_throttle_bow = 0.0;
    this->actuator_cmds_.thruster_throttle_stern = 0.0;

    // keep target 25 seconds or more away at all time, as long as sog is
    // greater than 10cm/s
    CORBA::Double target_sog = (d_to_target - LOITER_ARRIVAL_RADIUS) / 25.0;
    if (target_sog < 0.2) target_sog = 0.2;
    if (target_sog > this->sog_max_) target_sog = this->sog_max_;

    // let COLREG procedure override speed and course
    this->execute_colreg(target_pos, target_sog);

    CORBA::Double throttle = compute_throttle(target_sog);
    this->actuator_cmds_.engine_throttle_port = throttle;
    this->actuator_cmds_.engine_throttle_stbd = throttle;

    // steer using rudder
    CORBA::Double wheel = rudder_towards(pos, target_pos);
    CORBA::Double wheel_abs = std::abs(wheel);
    this->actuator_cmds_.rudder_angle = wheel;
    // assist with engines if at low throttle
    if (throttle < 0.6 && wheel_abs > 1.0) {
      if (throttle > 0.2) {
        // scale engine throttle difference linearly with wheel val
        CORBA::Double diff = 1 + (std::min(60.0, wheel_abs) / 60.0);
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Engine assist: %f\n"), diff));
        if (wheel > 0.0) {
          this->actuator_cmds_.engine_throttle_port *= diff;
          this->actuator_cmds_.engine_throttle_stbd /= diff;
        } else {
          this->actuator_cmds_.engine_throttle_port /= diff;
          this->actuator_cmds_.engine_throttle_stbd *= diff;
        }
      } else {
        // assist with bow thruster
        this->actuator_cmds_.rudder_angle = 0.0;
        CORBA::Double assist = std::min(wheel_abs / 60.0, 0.8);
        assist = (wheel / wheel_abs) * assist;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Bow thruster assist: %f\n"), assist));
        this->actuator_cmds_.thruster_throttle_bow = assist;
      }
    }

  } else {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("Executing loiter at:\n"
                        "    target position: %f, %f\n"
                        "    distance:        %f\n"
                        "    target bearing:  %f\n"
                        "    actual bearing:  %f\n"),
               target_pos.latitude, target_pos.longitude, d_to_target,
               target_bearing, bearing));
    // null out speed
    // Note: This doesn't work with sea current, actively maintaining
    //       position would require using thrusters and engines to
    //       move laterally/diagonally while maintaining bearing
    //       This could be done by inverting the cog vector and
    //       converting to lateral/axial speed vectors setpoints
    //       for PID control
    CORBA::Double throttle = compute_throttle(0.0);
    this->actuator_cmds_.engine_throttle_port = throttle;
    this->actuator_cmds_.engine_throttle_stbd = throttle;

    // maintain bearing
    // This could be improved by holding position by computing
    // relative bearing and distance to target, converting to cartesian
    // coordinates and scaling with magic numbers to convert to
    // throttle/thruster value.
    this->actuator_cmds_.rudder_angle = 0.0;
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Bow thruster throttle ")));
    this->actuator_cmds_.thruster_throttle_bow =
        this->bow_thruster_pid_.control(bearing, target_bearing);

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Stern thruster throttle ")));
    this->actuator_cmds_.thruster_throttle_stern =
        this->stern_thruster_pid_.control(bearing, target_bearing);
  }

  return true;
}

CORBA::Boolean AutopilotController::execute_dive() {
  if (!this->sensor_vals_set_) return false;

  this->tgt_depth_ = this->active_dive_procedure_.depth;
  this->execute_maintain_depth();

  ACE_hrtime_t now = ACE_OS::gethrtime();
  CORBA::Double depth_delta =
      std::abs(this->tgt_depth_adjusted_ - this->sensor_vals_.ship_depth);
  if (depth_delta > this->DEPTH_TOLERANCE) {
    this->last_outside_depth_interval_ts = now;
  }

  if (now - this->last_outside_depth_interval_ts >
      this->DEPTH_TOLERANCE_TIMEOUT) {
    // we've been in the interval for long enough that we can hope
    // the ballast tank pump PID controller stabilized depth enough,
    // consider the action completed
    this->action_completed_ = true;
    if (this->previous_state_ == Autopilot::AS_ROUTE_SUSPENDED) {
      this->set_state(Autopilot::AS_ROUTE_SUSPENDED);
    } else {
      this->set_state(Autopilot::AS_DISABLED);
    }
  }
  return true;
}

CORBA::Boolean AutopilotController::execute_maintain_depth() {
  if (!this->sensor_vals_set_) return false;

  CORBA::Double current_depth = this->sensor_vals_.ship_depth;
  // Don't dive to target depth if it means colliding with
  // the seafloor
  this->tgt_depth_adjusted_ =
      std::min(this->tgt_depth_, this->MIN_DEPTH_OFFSET +
                                     this->sensor_vals_.depth_under_keel +
                                     current_depth);

  CORBA::Double error = tgt_depth_adjusted_ - current_depth;
  // ACE_DEBUG(
  //     (LM_DEBUG, ACE_TEXT("Buoyancy: %f\n"), this->sensor_vals_.buoyancy));
  if (error >= 0 && this->sensor_vals_.buoyancy > 1.002) {
    // vessel is above target and significantly less massive than
    // the water it displaces: we can run the pump at 100% to quickly
    // reduce buoyancy, to avoid needlessly increasing the controller integral
    // term when filling an empty ballast tank
    // as a side-effect this also pre-fills the tank when operating at
    // the surface, so that diving is faster later
    // NOTE: this might break for pumps with a very high throughput (or with
    // vessels with a different mass?)
    this->actuator_cmds_.ballast_tank_pump = 1.0;
    return true;
  }

  // buoyancy is close to neutral or negative, let PID controller handle the
  // pump
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("Ballast tank pump throttle ")));
  CORBA::Double pid_output =
      this->ballast_tank_pid_.control(current_depth, this->tgt_depth_adjusted_);

  // hard cap on buoyancy, we don't ever want to empty or fill the tank to a
  // degree where we introduce too much momentum and end up constantly
  // overshooting the depth setpoint. this hack could probably be avoided
  // through proper tuning
  if (this->sensor_vals_.buoyancy > 1.0004 && pid_output < 0.0 &&
      this->current_state_ != Autopilot::AS_EMERGENCY_STOP) {
    this->actuator_cmds_.ballast_tank_pump = 0.0;
  } else if (this->sensor_vals_.buoyancy < 0.9996 && pid_output > 0.0) {
    this->actuator_cmds_.ballast_tank_pump = 0.0;
  } else {
    this->actuator_cmds_.ballast_tank_pump = pid_output;
  }

  return true;
}

CORBA::Boolean AutopilotController::execute_stop() {
  if (!this->sensor_vals_set_) return false;

  // force the vessel to surface
  this->tgt_depth_ = 0.0;

  this->actuator_cmds_.rudder_angle = 0.0;
  this->actuator_cmds_.thruster_throttle_bow = 0.0;
  this->actuator_cmds_.thruster_throttle_stern = 0.0;

  if (this->sensor_vals_.speed_over_ground < 0.05 &&
      this->sensor_vals_.ship_depth == 0.0) {
    // close enough to a stop, kill engines, disengage autopilot
    this->actuator_cmds_.engine_throttle_port = 0.0;
    this->actuator_cmds_.engine_throttle_stbd = 0.0;
    this->update_state(Autopilot::AC_ROUTE_STOP, true);
    return true;
  }
  CORBA::Double throttle = this->compute_throttle(0.0);
  this->actuator_cmds_.engine_throttle_port = throttle;
  this->actuator_cmds_.engine_throttle_stbd = throttle;

  return true;
}

PhysicalState::Actuators AutopilotController::get_actuator_cmds() {
  return this->actuator_cmds_;
}

Autopilot::Coordinates AutopilotController::get_position() {
  return estimated_position_;
}

CORBA::Double AutopilotController::rudder_towards(Autopilot::Coordinates pos,
                                                  Autopilot::Coordinates wpt) {
  CORBA::Double cog = this->sensor_vals_.course_over_ground;
  CORBA::Double rot = this->sensor_vals_.rate_of_turn * RAD_TO_DEG;

  // Calculate angle to waypoint
  double bearing = relative_bearing(cog, pos.latitude, pos.longitude,
                                    wpt.latitude, wpt.longitude);

  if (bearing >= 180.0) bearing -= 360.0;
  if (bearing <= -180.0) bearing += 360.0;

  double bearing_orig = bearing;

  // dampening of the final result is inversely proportional to the current
  // bearing error
  CORBA::Double dampening =
      1 / std::max(std::abs(bearing), 1.0);  // clamped between 0-1

  // Account for current rate of turn: reduce bearing error proportionally to
  // ROT, effectively countersteering when error is small and ROT is high
  bearing -= rot * DAMPENING_ROT;

  // rudder angle is proportional to bearing error clamped between +-60deg,
  // scaled from (-60, 60) --> (-30, 30), maximum rudder extent in BC
  CORBA::Double angle = std::min(std::max(bearing, -60.0), 60.0) / 60 * 30;

  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("Steering:\n"
                      "    bearing:   %f\n"
                      "    rot:       %f\n"
                      "    steering:  %f\n"
                      "    dampening: %f\n"
                      "    angle:   %f\n"),
             bearing_orig, rot, bearing, dampening, angle));

  return angle * (1 - dampening);
}

CORBA::Double AutopilotController::compute_throttle(
    CORBA::Double sog_setpoint) {
  CORBA::Double sog = this->sensor_vals_.speed_over_ground;
  // sog is always positive, check axial speed to determine if we're reversing
  if (this->sensor_vals_.speed < 0.0) {
    sog *= -1.0;
  }
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("Engine throttle ")));
  return this->engine_throttle_pid_.control(sog, sog_setpoint);
}
