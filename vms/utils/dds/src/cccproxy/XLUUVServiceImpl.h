#ifndef XLUUVSERVICE_IMPL_H
#define XLUUVSERVICE_IMPL_H

#include <grpcpp/support/status.h>

#include "./messages.grpc.pb.h"
#include "AutopilotTypeSupportC.h"
class XLUUVServiceImpl final : public XLUUV::Service {
 public:
  XLUUVServiceImpl(const Autopilot::RouteDataWriter_var,
                 const Autopilot::AutopilotCommandDataWriter_var,
                 const Autopilot::LoiterPositionDataWriter_var,
                 const Autopilot::DiveProcedureDataWriter_var,
                 const Autopilot::MissionDataWriter_var,
                 const Autopilot::MissionCommandDataWriter_var,
                 const Autopilot::ProcedureActivationDataWriter_var);

  grpc::Status SendMission(grpc::ServerContext*, const ApMission*,
                           google::protobuf::Empty*) override;
  grpc::Status SendLoiterPosition(grpc::ServerContext*, const LoiterPosition*,
                                  google::protobuf::Empty*) override;
  grpc::Status SendRoute(grpc::ServerContext*, const ApRoute*,
                         google::protobuf::Empty*) override;
  grpc::Status SendDiveProcedure(grpc::ServerContext*, const DiveProcedure*,
                                 google::protobuf::Empty*) override;
  grpc::Status SendMissionCommand(grpc::ServerContext*, const MsCommand*,
                                  google::protobuf::Empty*) override;
  grpc::Status SendApCommand(grpc::ServerContext*, const ApCommand*,
                             google::protobuf::Empty*) override;
  grpc::Status ActivateRoute(grpc::ServerContext*, const RouteId*,
                             google::protobuf::Empty*) override;
  grpc::Status ActivateLoiterPosition(grpc::ServerContext*,
                                      const LoiterPositionId*,
                                      google::protobuf::Empty*) override;
  grpc::Status ActivateDiveProcedure(grpc::ServerContext*,
                                     const DiveProcedureId*,
                                     google::protobuf::Empty*) override;

 private:
  const Autopilot::RouteDataWriter_var route_dw_;
  const Autopilot::AutopilotCommandDataWriter_var ap_cmd_dw_;
  const Autopilot::LoiterPositionDataWriter_var lp_dw_;
  const Autopilot::DiveProcedureDataWriter_var dp_dw_;
  const Autopilot::MissionDataWriter_var mission_dw_;
  const Autopilot::MissionCommandDataWriter_var ms_cmd_dw_;
  const Autopilot::ProcedureActivationDataWriter_var proc_act_dw_;
};
#endif
