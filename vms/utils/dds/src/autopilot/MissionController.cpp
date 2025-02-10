#include "MissionController.h"

#include <ace/Basic_Types.h>
#include <ace/OS_NS_time.h>

#include "AutopilotC.h"

MissionController::MissionController(AutopilotController *ap_controller) {
  this->ap_controller_ = ap_controller;
  this->status_ = Autopilot::MS_DISABLED;
  this->mission_set_ = false;
  this->report_available_ = false;
  this->report_ = Autopilot::MissionReport();
}

void MissionController::set_mission(Autopilot::Mission ap_mission) {
  if (ap_mission.mission_items.length() == 0) {
    // don't store missions with no items
    return;
  }
  this->mission_ = ap_mission;
  this->mission_items_ = this->mission_.mission_items;
  this->mission_set_ = true;
  // changing the mission stops it: we don't support updating in-progress
  // missions
  this->execute_command(Autopilot::MC_STOP);
}

CORBA::Boolean MissionController::is_report_available() {
  return this->report_available_;
}

Autopilot::MissionReport MissionController::get_report() {
  if (this->mission_set_) {
    this->report_.name = this->mission_.name;
    this->report_.progress = this->mission_index_ + 1;
    this->report_.length = this->mission_items_.length();
  }
  this->report_.status = this->status_;

  this->report_available_ = false;
  this->last_report_ts_ = ACE_OS::gethrtime();

  return this->report_;
}

void MissionController::execute_command(Autopilot::MissionCommandType command) {
  switch (this->status_) {
    case Autopilot::MS_DISABLED: {
      switch (command) {
        case Autopilot::MC_START: {
          if (this->mission_set_) {
            this->status_ = Autopilot::MS_ENABLED;
            ACE_DEBUG(
                (LM_DEBUG,
                 ACE_TEXT("Starting Mission\nExecuting mission item %i/%i\n"),
                 this->mission_index_ + 1, this->mission_items_.length()));
            this->execute_item(this->mission_items_[this->mission_index_]);
          }
          break;
        }
        case Autopilot::MC_STOP: {
          this->mission_index_ = 0;
          break;
        }
        default: {
          // suspending, resuming or skipping a step doesn't work
        }
      }
      break;
    }
    case Autopilot::MS_ENABLED: {
      switch (command) {
        case Autopilot::MC_STOP: {
          this->status_ = Autopilot::MS_DISABLED;
          this->mission_index_ = 0;
          break;
        }
        case Autopilot::MC_SUSPEND: {
          this->status_ = Autopilot::MS_SUSPENDED;
          this->pause_start_ts_ = ACE_OS::gethrtime();
          break;
        }
        case Autopilot::MC_SKIP_STEP: {
          this->mission_index_++;
          if (this->mission_index_ == this->mission_items_.length()) {
            this->execute_command(Autopilot::MC_STOP);
          } else {
            this->execute_item(this->mission_items_[mission_index_]);
          }
          break;
        }
        default: {
          // start and resume do nothing
        }
      }
      break;
    }
    case Autopilot::MS_SUSPENDED: {
      switch (command) {
        case Autopilot::MC_STOP: {
          this->status_ = Autopilot::MS_DISABLED;
          this->mission_index_ = 0;
          break;
        }
        case Autopilot::MC_RESUME: {
          this->status_ = Autopilot::MS_ENABLED;
          ACE_hrtime_t now = ACE_OS::gethrtime();
          this->item_start_ts_ =
              now - (this->pause_start_ts_ - this->item_start_ts_);
          break;
        }
        default: {
          // don't allow starting or skipping steps,
          // suspending again doesn't do anything
        }
      }
      break;
    }

    case Autopilot::MS_UNKNOWN: {
      // should never happen, drop silently
      break;
    }
  }

  // always send a report to inform about a possible state change
  this->report_available_ = true;
}

void MissionController::execute_item(Autopilot::MissionItem item) {
  // starting a new item, notify CCC
  this->report_available_ = true;

  ACE_hrtime_t now = ACE_OS::gethrtime();
  this->item_start_ts_ = now;
  this->item_timeout_ =
      item.timeout >= 0 ? (ACE_hrtime_t)(item.timeout * 1e9) : ACE_UINT64_MAX;

  switch (item.action._d()) {
    case Autopilot::MIAT_ACT_LP: {
      this->ap_controller_->activate_procedure(
          Autopilot::ProcedureActivation(
              {Autopilot::PROC_LOITERPOSITION, item.action.loiter_pos_id()}),
          true);
      break;
    }
    case Autopilot::MIAT_ACT_ROUTE: {
      this->ap_controller_->activate_procedure(
          Autopilot::ProcedureActivation(
              {Autopilot::PROC_ROUTE, item.action.route_id()}),
          true);
      break;
    }
    case Autopilot::MIAT_ACT_DP: {
      this->ap_controller_->activate_procedure(
          Autopilot::ProcedureActivation(
              {Autopilot::PROC_DIVEPROCEDURE, item.action.dive_proc_id()}),
          true);
      break;
    }
    case Autopilot::MIAT_SET_AP_CMD: {
      this->ap_controller_->update_state(item.action.ap_cmd(), true);
      break;
    }
  }
}

CORBA::Boolean MissionController::run() {
  ACE_hrtime_t now = ACE_OS::gethrtime();
  // have we sent a report recently?
  if (now - this->last_report_ts_ >= REPORT_INTERVAL) {
    this->report_available_ = true;
  }

  if (this->status_ == Autopilot::MS_DISABLED ||
      this->status_ == Autopilot::MS_SUSPENDED) {
    return false;
  }

  Autopilot::MissionItem current_item = this->mission_items_[mission_index_];
  CORBA::Boolean item_complete = false;

  if (current_item.until_completion) {
    item_complete = this->ap_controller_->is_action_completed();
  } else {
    // check if enough time has elapsed
    if (now - this->item_start_ts_ >= this->item_timeout_) item_complete = true;
  }

  if (item_complete) {
    this->mission_index_++;
    if (this->mission_index_ == this->mission_items_.length()) {
      this->execute_command(Autopilot::MC_STOP);
    } else {
      ACE_DEBUG((LM_DEBUG, ACE_TEXT("Executing mission item %i/%i\n"),
                 this->mission_index_ + 1, this->mission_items_.length()));
      this->execute_item(this->mission_items_[mission_index_]);
    }
  }

  return false;
}
