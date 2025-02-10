#include <ace/ace_wchar.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/Service_Participant.h>
#include <dds/DdsDcpsDomainC.h>
#include <dds/DdsDcpsInfrastructureC.h>
#include <dds/DdsDcpsSubscriptionC.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/support/status.h>
#include <tao/Basic_Types.h>
#include <tao/Exception.h>

#include <algorithm>
#include <string>

#include "./ActuatorsDRLImpl.h"
#include "./SensorsDRLImpl.h"
#include "./XLUUVServiceImpl.h"
#include "APReportDRLImpl.h"
#include "AivdmMessageDRLImpl.h"
#include "AutopilotTypeSupportC.h"
#include "AutopilotTypeSupportImpl.h"
#include "ColregStatusDRLImpl.h"
#include "MissionReportDRLImpl.h"
#include "PhysicalStateTypeSupportC.h"
#include "PhysicalStateTypeSupportImpl.h"
#include "grpcpp/security/server_credentials.h"

#ifdef ACE_AS_STATIC_LIBS
#endif

int ACE_TMAIN(int argc, ACE_TCHAR* argv[]) {
  try {
    std::string grpc_listen_port = "42001";
    std::string grpc_listen_host = "0.0.0.0";
    std::string ccc_telemetry_host = "127.0.0.1";
    std::string ccc_telemetry_port = "43002";

    ACE_TCHAR** argv_end = argv + argc;

    // Argument parsing
    char** grpc_listen_port_arg =
        std::min(std::find(argv, argv_end, std::string("-gp")),
                 std::find(argv, argv_end, std::string("--grpc-port")));
    if (grpc_listen_port_arg < argv_end && ++grpc_listen_port_arg < argv_end) {
      grpc_listen_port = std::string(*grpc_listen_port_arg);
    }

    char** ccc_telemetry_port_arg = std::min(
        std::find(argv, argv_end, std::string("-ctp")),
        std::find(argv, argv_end, std::string("--ccc-telemetry-port")));
    if (ccc_telemetry_port_arg < argv_end &&
        ++ccc_telemetry_port_arg < argv_end) {
      ccc_telemetry_port = std::string(*ccc_telemetry_port_arg);
    }

    char** ccc_telemetry_host_arg = std::min(
        std::find(argv, argv_end, std::string("-cth")),
        std::find(argv, argv_end, std::string("--ccc-telemetry-host")));
    if (ccc_telemetry_host_arg < argv_end &&
        ++ccc_telemetry_host_arg < argv_end) {
      ccc_telemetry_host = std::string(*ccc_telemetry_host_arg);
    }
    ACE_DEBUG(
        (LM_DEBUG,
         ACE_TEXT("Parsed args: listen to CCC on %s:%s, send telemetry to "
                  "CCC on %s:%s\n"),
         grpc_listen_host.c_str(), grpc_listen_port.c_str(),
         ccc_telemetry_host.c_str(), ccc_telemetry_port.c_str()));

    // Setup DDS listeners
    // Create the participant
    DDS::DomainParticipantFactory_var dpf =
        TheParticipantFactoryWithArgs(argc, argv);
    DDS::DomainParticipant_var participant = dpf->create_participant(
        42, PARTICIPANT_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!participant) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - create_participant failed!\n")),
          1);
    }

    // Type support registration
    PhysicalState::SensorsTypeSupport_var sensors_servant =
        new PhysicalState::SensorsTypeSupportImpl();
    PhysicalState::ActuatorsTypeSupport_var actuators_servant =
        new PhysicalState::ActuatorsTypeSupportImpl();
    PhysicalState::AivdmMessageTypeSupport_var aivdm_servant =
        new PhysicalState::AivdmMessageTypeSupportImpl();
    Autopilot::APReportTypeSupport_var ap_report_servant =
        new Autopilot::APReportTypeSupportImpl();
    Autopilot::MissionReportTypeSupport_var ms_report_servant =
        new Autopilot::MissionReportTypeSupportImpl();
    Autopilot::ColregStatusTypeSupport_var colreg_status_servant =
        new Autopilot::ColregStatusTypeSupportImpl();

    Autopilot::RouteTypeSupport_var route_servant =
        new Autopilot::RouteTypeSupportImpl();
    Autopilot::AutopilotCommandTypeSupport_var autopilot_command_servant =
        new Autopilot::AutopilotCommandTypeSupportImpl();
    Autopilot::LoiterPositionTypeSupport_var loiter_position_servant =
        new Autopilot::LoiterPositionTypeSupportImpl();
    Autopilot::DiveProcedureTypeSupport_var dive_proc_servant =
        new Autopilot::DiveProcedureTypeSupportImpl();
    Autopilot::MissionTypeSupport_var mission_servant =
        new Autopilot::MissionTypeSupportImpl();
    Autopilot::MissionCommandTypeSupport_var mission_cmd_servant =
        new Autopilot::MissionCommandTypeSupportImpl();
    Autopilot::ProcedureActivationTypeSupport_var proc_act_servant =
        new Autopilot::ProcedureActivationTypeSupportImpl();

    // clang-format off
    if (route_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        aivdm_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        sensors_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        mission_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        proc_act_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        dive_proc_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        ap_report_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        ms_report_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        actuators_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        mission_cmd_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        colreg_status_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        loiter_position_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        autopilot_command_servant->register_type(participant, "") != DDS::RETCODE_OK) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - register_type failed!\n")),
          1);
    }
    // clang-format on

    // Create topics
    CORBA::String_var sensors_type_name = sensors_servant->get_type_name();
    DDS::Topic_var sensors_topic = participant->create_topic(
        "BC Sensors", sensors_type_name, TOPIC_QOS_DEFAULT, 0,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    CORBA::String_var actuators_type_name = actuators_servant->get_type_name();
    DDS::Topic_var actuators_topic = participant->create_topic(
        "BC Actuators", actuators_type_name, TOPIC_QOS_DEFAULT, 0,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    CORBA::String_var aivdm_type_name = aivdm_servant->get_type_name();
    DDS::Topic_var aivdm_topic = participant->create_topic(
        "AIS Messages", aivdm_type_name, TOPIC_QOS_DEFAULT, 0,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    CORBA::String_var ms_report_type_name = ms_report_servant->get_type_name();
    DDS::Topic_var ms_report_topic = participant->create_topic(
        "Mission Reports", ms_report_type_name, TOPIC_QOS_DEFAULT, 0,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    CORBA::String_var ap_report_type_name = ap_report_servant->get_type_name();
    DDS::Topic_var ap_report_topic = participant->create_topic(
        "Autopilot Reports", ap_report_type_name, TOPIC_QOS_DEFAULT, 0,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    CORBA::String_var colreg_status_type_name =
        colreg_status_servant->get_type_name();
    DDS::Topic_var colreg_status_topic = participant->create_topic(
        "Autopilot COLREG status", colreg_status_type_name, TOPIC_QOS_DEFAULT,
        0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    CORBA::String_var route_type_name = route_servant->get_type_name();
    DDS::Topic_var route_topic = participant->create_topic(
        "Autopilot Routes", route_type_name, TOPIC_QOS_DEFAULT, 0,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    CORBA::String_var autopilot_command_type_name =
        autopilot_command_servant->get_type_name();
    DDS::Topic_var autopilot_command_topic = participant->create_topic(
        "Autopilot Commands", autopilot_command_type_name, TOPIC_QOS_DEFAULT, 0,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    CORBA::String_var loiter_position_type_name =
        loiter_position_servant->get_type_name();
    DDS::Topic_var loiter_position_topic = participant->create_topic(
        "Autopilot Loiter Positions", loiter_position_type_name,
        TOPIC_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    CORBA::String_var dive_proc_type_name = dive_proc_servant->get_type_name();
    DDS::Topic_var dive_proc_topic = participant->create_topic(
        "Autopilot Dive Procedures", dive_proc_type_name, TOPIC_QOS_DEFAULT, 0,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    CORBA::String_var mission_type_name = mission_servant->get_type_name();
    DDS::Topic_var mission_topic = participant->create_topic(
        "Autopilot Mission Data", mission_type_name, TOPIC_QOS_DEFAULT, 0,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    CORBA::String_var mission_cmd_type_name =
        mission_cmd_servant->get_type_name();
    DDS::Topic_var mission_cmd_topic = participant->create_topic(
        "Autopilot Mission Commands", mission_cmd_type_name, TOPIC_QOS_DEFAULT,
        0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    CORBA::String_var proc_act_type_name = proc_act_servant->get_type_name();
    DDS::Topic_var proc_act_topic = participant->create_topic(
        "Autopilot Procedure Activations", proc_act_type_name,
        TOPIC_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!sensors_topic || !actuators_topic || !aivdm_topic ||
        !ap_report_topic || !ms_report_topic || !loiter_position_topic ||
        !dive_proc_topic || !autopilot_command_topic || !route_topic ||
        !mission_topic || !mission_cmd_topic || !proc_act_topic ||
        !colreg_status_topic) {
      ACE_ERROR_RETURN(
          (LM_ERROR, ACE_TEXT("ERROR: %N:%l: main() - create_topic failed!\n")),
          1);
    }

    // create subscriber for all report topics
    DDS::Subscriber_var subscriber = participant->create_subscriber(
        SUBSCRIBER_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    DDS::DataReaderQos reader_qos;
    subscriber->get_default_datareader_qos(reader_qos);
    reader_qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;

    if (!subscriber) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - create_subscriber failed!\n")),
          1);
    }

    // DataReaders
    DDS::DataReaderListener_var sensors_listener(
        new SensorsDataReaderListenerImpl(ccc_telemetry_host,
                                          ccc_telemetry_port));

    DDS::DataReader_var sensors_dr = subscriber->create_datareader(
        sensors_topic, reader_qos, sensors_listener,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    DDS::DataReaderListener_var actuators_listener(
        new ActuatorsDataReaderListenerImpl(ccc_telemetry_host,
                                            ccc_telemetry_port));

    DDS::DataReader_var actuators_dr = subscriber->create_datareader(
        actuators_topic, reader_qos, actuators_listener,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    DDS::DataReaderListener_var aivdm_listener(
        new AivdmMessageDataReaderListenerImpl(ccc_telemetry_host,
                                               ccc_telemetry_port));

    DDS::DataReader_var aivdm_dr =
        subscriber->create_datareader(aivdm_topic, reader_qos, aivdm_listener,
                                      OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    DDS::DataReaderListener_var ap_report_listener(
        new APReportDataReaderListenerImpl(ccc_telemetry_host,
                                           ccc_telemetry_port));

    DDS::DataReader_var ap_report_dr = subscriber->create_datareader(
        ap_report_topic, reader_qos, ap_report_listener,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    DDS::DataReaderListener_var colreg_status_listener(
        new ColregStatusDataReaderListenerImpl(ccc_telemetry_host,
                                               ccc_telemetry_port));

    DDS::DataReader_var colreg_status_dr = subscriber->create_datareader(
        colreg_status_topic, reader_qos, colreg_status_listener,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    DDS::DataReaderListener_var ms_report_listener(
        new MissionReportDataReaderListenerImpl(ccc_telemetry_host,
                                                ccc_telemetry_port));

    DDS::DataReader_var ms_report_dr = subscriber->create_datareader(
        ms_report_topic, reader_qos, ms_report_listener,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!actuators_dr || !aivdm_dr || !sensors_dr || !ap_report_dr ||
        !ms_report_dr || !colreg_status_dr) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - create_datareader failed!\n")),
          1);
    }

    // Create publisher
    DDS::Publisher_var publisher = participant->create_publisher(
        PUBLISHER_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!publisher) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - create_publisher failed!\n")),
          1);
    }

    // create the writers
    DDS::DataWriter_var route_base_dw =
        publisher->create_datawriter(route_topic, DATAWRITER_QOS_DEFAULT, 0,
                                     OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    DDS::DataWriter_var autopilot_command_base_dw =
        publisher->create_datawriter(autopilot_command_topic,
                                     DATAWRITER_QOS_DEFAULT, 0,
                                     OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    DDS::DataWriter_var loiter_position_base_dw = publisher->create_datawriter(
        loiter_position_topic, DATAWRITER_QOS_DEFAULT, 0,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    DDS::DataWriter_var dive_proc_base_dw =
        publisher->create_datawriter(dive_proc_topic, DATAWRITER_QOS_DEFAULT, 0,
                                     OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    DDS::DataWriter_var mission_base_dw =
        publisher->create_datawriter(mission_topic, DATAWRITER_QOS_DEFAULT, 0,
                                     OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    DDS::DataWriter_var mission_cmd_base_dw =
        publisher->create_datawriter(mission_cmd_topic, DATAWRITER_QOS_DEFAULT,
                                     0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    DDS::DataWriter_var proc_act_base_dw =
        publisher->create_datawriter(proc_act_topic, DATAWRITER_QOS_DEFAULT, 0,
                                     OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!route_base_dw || !autopilot_command_base_dw ||
        !loiter_position_base_dw || !dive_proc_base_dw || !mission_base_dw ||
        !mission_cmd_base_dw || !proc_act_base_dw) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - create_datawriter failed!\n")),
          1);
    }

    // narrow dw types
    Autopilot::RouteDataWriter_var route_dw =
        Autopilot::RouteDataWriter::_narrow(route_base_dw);
    Autopilot::AutopilotCommandDataWriter_var autopilot_command_dw =
        Autopilot::AutopilotCommandDataWriter::_narrow(
            autopilot_command_base_dw);
    Autopilot::LoiterPositionDataWriter_var loiter_position_dw =
        Autopilot::LoiterPositionDataWriter::_narrow(loiter_position_base_dw);
    Autopilot::DiveProcedureDataWriter_var dive_proc_dw =
        Autopilot::DiveProcedureDataWriter::_narrow(dive_proc_base_dw);
    Autopilot::MissionDataWriter_var mission_dw =
        Autopilot::MissionDataWriter::_narrow(mission_base_dw);
    Autopilot::MissionCommandDataWriter_var mission_cmd_dw =
        Autopilot::MissionCommandDataWriter::_narrow(mission_cmd_base_dw);
    Autopilot::ProcedureActivationDataWriter_var proc_act_dw =
        Autopilot::ProcedureActivationDataWriter::_narrow(proc_act_base_dw);

    if (!route_dw || !autopilot_command_dw || !loiter_position_dw ||
        !mission_dw || !mission_cmd_dw || !dive_proc_dw || !proc_act_dw) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - _narrow(writer) failed!\n")),
          1);
    }

    // start the gRPC service
    XLUUVServiceImpl service{
        route_dw,   autopilot_command_dw, loiter_position_dw, dive_proc_dw,
        mission_dw, mission_cmd_dw,       proc_act_dw};
    grpc::EnableDefaultHealthCheckService(true);
    // we're currently building without reflection and probably don't need it
    // grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    std::string server_address{grpc_listen_host + ":" + grpc_listen_port};
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    // blocking until server is shut down
    server->Wait();
  } catch (const CORBA::Exception& e) {
    e._tao_print_exception("Exception caught in main():");
    return 1;
  }
}
