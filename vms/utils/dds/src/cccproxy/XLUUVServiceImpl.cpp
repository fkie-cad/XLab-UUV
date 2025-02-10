#include "XLUUVServiceImpl.h"

#include <dds/DdsDcpsInfrastructureC.h>

#include "AutopilotC.h"
#include "google/protobuf/empty.pb.h"

XLUUVServiceImpl::XLUUVServiceImpl(
    const Autopilot::RouteDataWriter_var route_dw,
    const Autopilot::AutopilotCommandDataWriter_var ap_cmd_dw,
    const Autopilot::LoiterPositionDataWriter_var lp_dw,
    const Autopilot::DiveProcedureDataWriter_var dp_dw,
    const Autopilot::MissionDataWriter_var mission_dw,
    const Autopilot::MissionCommandDataWriter_var ms_cmd_dw,
    const Autopilot::ProcedureActivationDataWriter_var proc_act_dw)
    : route_dw_{route_dw},
      ap_cmd_dw_{ap_cmd_dw},
      lp_dw_{lp_dw},
      dp_dw_{dp_dw},
      mission_dw_{mission_dw},
      ms_cmd_dw_{ms_cmd_dw},
      proc_act_dw_{proc_act_dw} {}

grpc::Status XLUUVServiceImpl::SendMission(grpc::ServerContext* context,
                                         const ApMission* request,
                                         google::protobuf::Empty* response) {
  Autopilot::Mission mission;
  mission.id = request->id();
  mission.name = request->name().c_str();
  mission.mission_items.length(request->ap_mission_items_size());
  const google::protobuf::RepeatedPtrField< ::ApMission_ApMissionItem>& items =
      request->ap_mission_items();

  for (int i = 0; i < request->ap_mission_items_size(); ++i) {
    ApMission_ApMissionItem item = items[i];
    CORBA::Boolean until_completion = false;
    CORBA::Long timeout = -1;
    if (item.has_until_completion()) {
      until_completion = item.until_completion();
    } else if (item.has_until_timeout()) {
      timeout = item.until_timeout();
    }

    Autopilot::MissionItemAction action = Autopilot::MissionItemAction();
    if (item.has_act_route_id()) {
      action.route_id(item.act_route_id());
    } else if (item.has_act_diveproc_id()) {
      action.dive_proc_id(item.act_diveproc_id());
    } else if (item.has_act_loiterpos_id()) {
      action.loiter_pos_id(item.act_loiterpos_id());
    } else if (item.has_set_ap_cmd()) {
      // WARNING: we're relying on the 1:1 match between enum values here
      action.ap_cmd(Autopilot::AutopilotCommandType(item.set_ap_cmd()));
    }
    mission.mission_items[i] =
        Autopilot::MissionItem({until_completion, timeout, action});
  }
  this->mission_dw_->write(mission, DDS::HANDLE_NIL);
  return grpc::Status::OK;
}

grpc::Status XLUUVServiceImpl::SendLoiterPosition(
    grpc::ServerContext* context, const LoiterPosition* request,
    google::protobuf::Empty* response) {
  Autopilot::LoiterPosition loiter_position;
  loiter_position.id = request->id();
  loiter_position.bearing = request->bearing();
  loiter_position.position = Autopilot::Waypoint();
  loiter_position.position.name = request->position().name().c_str();
  loiter_position.position.coords =
      Autopilot::Coordinates({request->position().coords().latitude(),
                              request->position().coords().longitude()});

  this->lp_dw_->write(loiter_position, DDS::HANDLE_NIL);
  return grpc::Status::OK;
}

grpc::Status XLUUVServiceImpl::SendRoute(grpc::ServerContext* context,
                                       const ApRoute* request,
                                       google::protobuf::Empty* response) {
  Autopilot::Route route;
  route.id = request->id();
  route.name = request->name().c_str();
  route.planned_speed = request->planned_speed();
  route.waypoints.length(request->waypoints_size());
  const google::protobuf::RepeatedPtrField<Waypoint>& items =
      request->waypoints();

  for (int i = 0; i < request->waypoints_size(); ++i) {
    Waypoint item = items[i];
    Autopilot::Waypoint wpt;
    wpt.coords = Autopilot::Coordinates(
        {item.coords().latitude(), item.coords().longitude()});
    wpt.name = item.name().c_str();
    route.waypoints[i] = wpt;
  }

  this->route_dw_->write(route, DDS::HANDLE_NIL);

  return grpc::Status::OK;
}

grpc::Status XLUUVServiceImpl::SendDiveProcedure(
    grpc::ServerContext* context, const DiveProcedure* request,
    google::protobuf::Empty* response) {
  Autopilot::DiveProcedure dive_procedure;
  dive_procedure.id = request->id();
  dive_procedure.name = request->name().c_str();
  dive_procedure.depth = request->depth();

  this->dp_dw_->write(dive_procedure, DDS::HANDLE_NIL);

  return grpc::Status::OK;
}

grpc::Status XLUUVServiceImpl::SendMissionCommand(
    grpc::ServerContext* context, const MsCommand* request,
    google::protobuf::Empty* response) {
  Autopilot::MissionCommand command;
  command.command = Autopilot::MissionCommandType(request->command());

  this->ms_cmd_dw_->write(command, DDS::HANDLE_NIL);

  return grpc::Status::OK;
}

grpc::Status XLUUVServiceImpl::SendApCommand(grpc::ServerContext* context,
                                           const ApCommand* request,
                                           google::protobuf::Empty* response) {
  Autopilot::AutopilotCommand command;
  command.command = Autopilot::AutopilotCommandType(request->command());

  this->ap_cmd_dw_->write(command, DDS::HANDLE_NIL);

  return grpc::Status::OK;
}

grpc::Status XLUUVServiceImpl::ActivateRoute(grpc::ServerContext* context,
                                           const RouteId* request,
                                           google::protobuf::Empty* response) {
  Autopilot::ProcedureActivation proc_act;
  proc_act.procedure = Autopilot::PROC_ROUTE;
  proc_act.procedure_id = request->id();

  this->proc_act_dw_->write(proc_act, DDS::HANDLE_NIL);

  return grpc::Status::OK;
}

grpc::Status XLUUVServiceImpl::ActivateLoiterPosition(
    grpc::ServerContext* context, const LoiterPositionId* request,
    google::protobuf::Empty* response) {
  Autopilot::ProcedureActivation proc_act;
  proc_act.procedure = Autopilot::PROC_LOITERPOSITION;
  proc_act.procedure_id = request->id();

  this->proc_act_dw_->write(proc_act, DDS::HANDLE_NIL);

  return grpc::Status::OK;
}

grpc::Status XLUUVServiceImpl::ActivateDiveProcedure(
    grpc::ServerContext* context, const DiveProcedureId* request,
    google::protobuf::Empty* response) {
  Autopilot::ProcedureActivation proc_act;
  proc_act.procedure = Autopilot::PROC_DIVEPROCEDURE;
  proc_act.procedure_id = request->id();

  this->proc_act_dw_->write(proc_act, DDS::HANDLE_NIL);

  return grpc::Status::OK;
}
