#include <ace/Log_Msg.h>
#include <ace/Time_Value.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/StaticIncludes.h>
#include <dds/DCPS/WaitSet.h>
#include <dds/DdsDcpsCoreC.h>
#include <dds/DdsDcpsInfrastructureC.h>
#include <dds/DdsDcpsPublicationC.h>
#include <tao/Basic_Types.h>

#include "AivdmMessageDRLImpl.h"
#include "AutopilotC.h"
#include "AutopilotController.h"
#include "AutopilotTypeSupportC.h"
#include "MissionCommandDRLImpl.h"
#include "MissionController.h"
#include "MissionDRLImpl.h"
#include "ProcedureActivationDRLImpl.h"
#ifdef ACE_AS_STATIC_LIBS
#include <dds/DCPS/RTPS/RtpsDiscovery.h>
#include <dds/DCPS/transport/rtps_udp/RtpsUdp.h>
#endif

#include "AutopilotCommandDRLImpl.h"
#include "AutopilotTypeSupportImpl.h"
#include "DiveProcedureDRLImpl.h"
#include "LoiterPositionDRLImpl.h"
#include "PhysicalStateTypeSupportImpl.h"
#include "RouteDRLImpl.h"
#include "SensorsDRLImpl.h"

int ACE_TMAIN(int argc, ACE_TCHAR *argv[]) {
  try {
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

    // Register type supports
    Autopilot::RouteTypeSupport_var route_servant =
        new Autopilot::RouteTypeSupportImpl();

    PhysicalState::SensorsTypeSupport_var sensors_servant =
        new PhysicalState::SensorsTypeSupportImpl();

    PhysicalState::ActuatorsTypeSupport_var actuators_servant =
        new PhysicalState::ActuatorsTypeSupportImpl();

    PhysicalState::AivdmMessageTypeSupport_var aivdm_servant =
        new PhysicalState::AivdmMessageTypeSupportImpl();

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

    Autopilot::MissionReportTypeSupport_var ms_report_servant =
        new Autopilot::MissionReportTypeSupportImpl();

    Autopilot::APReportTypeSupport_var ap_report_servant =
        new Autopilot::APReportTypeSupportImpl();

    Autopilot::ColregStatusTypeSupport_var colreg_status_servant =
        new Autopilot::ColregStatusTypeSupportImpl();

    // clang-format off
    if (route_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        aivdm_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        sensors_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        mission_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        proc_act_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        actuators_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        dive_proc_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        ms_report_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        ap_report_servant->register_type(participant, "") != DDS::RETCODE_OK ||
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

    // Create the topics
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

    if (!loiter_position_topic || !dive_proc_topic || !aivdm_topic ||
        !autopilot_command_topic || !route_topic || !mission_topic ||
        !mission_cmd_topic || !proc_act_topic || !sensors_topic ||
        !actuators_topic || !ms_report_topic || !ap_report_topic ||
        !colreg_status_topic) {
      ACE_ERROR_RETURN(
          (LM_ERROR, ACE_TEXT("ERROR: %N:%l: main() - create_topic failed!\n")),
          1);
    }

    // Create Subscriber for the Route, AutopilotCommand, LoiterPosition and
    // Mission, MissionCommand ProcActivation and Sensors topics
    DDS::Subscriber_var subscriber = participant->create_subscriber(
        SUBSCRIBER_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!subscriber) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - create_subscriber failed!\n")),
          1);
    }

    // Create the publisher for topics the autopilot writes to
    DDS::Publisher_var publisher = participant->create_publisher(
        PUBLISHER_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!publisher) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - create_publisher failed!\n")),
          1);
    }

    // Create the DataWriter for the Actuators and report topics
    DDS::DataWriter_var actuators_base_dw =
        publisher->create_datawriter(actuators_topic, DATAWRITER_QOS_DEFAULT, 0,
                                     OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    DDS::DataWriter_var ms_report_base_dw =
        publisher->create_datawriter(ms_report_topic, DATAWRITER_QOS_DEFAULT, 0,
                                     OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    DDS::DataWriter_var ap_report_base_dw =
        publisher->create_datawriter(ap_report_topic, DATAWRITER_QOS_DEFAULT, 0,
                                     OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    DDS::DataWriter_var colreg_status_base_dw = publisher->create_datawriter(
        colreg_status_topic, DATAWRITER_QOS_DEFAULT, 0,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!actuators_base_dw || !ms_report_base_dw || !ap_report_base_dw ||
        !colreg_status_base_dw) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - create_datawriter failed!\n")),
          1);
    }

    PhysicalState::ActuatorsDataWriter_var actuators_dw =
        PhysicalState::ActuatorsDataWriter::_narrow(actuators_base_dw);
    Autopilot::MissionReportDataWriter_var ms_report_dw =
        Autopilot::MissionReportDataWriter::_narrow(ms_report_base_dw);
    Autopilot::APReportDataWriter_var ap_report_dw =
        Autopilot::APReportDataWriter::_narrow(ap_report_base_dw);
    Autopilot::ColregStatusDataWriter_var colreg_status_dw =
        Autopilot::ColregStatusDataWriter::_narrow(colreg_status_base_dw);

    if (!actuators_dw || !ms_report_dw || !ap_report_dw || !colreg_status_dw) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - _narrow(writer) failed!\n")),
          1);
    }

    // Create the DataReaders
    DDS::DataReaderQos reader_qos;
    subscriber->get_default_datareader_qos(reader_qos);
    reader_qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;

    // Route DataReader
    DDS::DataReaderListener_var route_listener(new RouteDataReaderListenerImpl);

    RouteDataReaderListenerImpl *route_listener_servant =
        dynamic_cast<RouteDataReaderListenerImpl *>(route_listener.in());

    DDS::DataReader_var route_dr =
        subscriber->create_datareader(route_topic, reader_qos, route_listener,
                                      OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    // LoiterPosition DataReader
    DDS::DataReaderListener_var loiter_position_listener(
        new LoiterPositionDataReaderListenerImpl);

    LoiterPositionDataReaderListenerImpl *loiter_listener_servant =
        dynamic_cast<LoiterPositionDataReaderListenerImpl *>(
            loiter_position_listener.in());

    DDS::DataReader_var loiter_position_dr = subscriber->create_datareader(
        loiter_position_topic, reader_qos, loiter_position_listener,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    // DiveProcedure DataReader
    DDS::DataReaderListener_var dive_proc_listener(
        new DiveProcedureDataReaderListenerImpl);

    DiveProcedureDataReaderListenerImpl *dive_proc_listener_servant =
        dynamic_cast<DiveProcedureDataReaderListenerImpl *>(
            dive_proc_listener.in());

    DDS::DataReader_var dive_proc_dr = subscriber->create_datareader(
        dive_proc_topic, reader_qos, dive_proc_listener,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    // Mission DataReader
    DDS::DataReaderListener_var mission_listener(
        new MissionDataReaderListenerImpl);

    MissionDataReaderListenerImpl *mission_listener_servant =
        dynamic_cast<MissionDataReaderListenerImpl *>(mission_listener.in());

    DDS::DataReader_var mission_dr = subscriber->create_datareader(
        mission_topic, reader_qos, mission_listener,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    // Mission Command DataReader
    DDS::DataReaderListener_var mission_cmd_listener(
        new MissionCommandDataReaderListenerImpl);

    MissionCommandDataReaderListenerImpl *mission_cmd_listener_servant =
        dynamic_cast<MissionCommandDataReaderListenerImpl *>(
            mission_cmd_listener.in());

    DDS::DataReader_var mission_cmd_dr = subscriber->create_datareader(
        mission_cmd_topic, reader_qos, mission_cmd_listener,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    // Procedure Activation DataReader
    DDS::DataReaderListener_var proc_act_listener(
        new ProcedureActivationDataReaderListenerImpl);

    ProcedureActivationDataReaderListenerImpl *proc_act_listener_servant =
        dynamic_cast<ProcedureActivationDataReaderListenerImpl *>(
            proc_act_listener.in());

    DDS::DataReader_var proc_act_dr = subscriber->create_datareader(
        proc_act_topic, reader_qos, proc_act_listener,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    // AutopilotCommands DataReader
    DDS::DataReaderListener_var ap_command_listener(
        new AutopilotCommandDataReaderListenerImpl);

    AutopilotCommandDataReaderListenerImpl *ap_command_listener_servant =
        dynamic_cast<AutopilotCommandDataReaderListenerImpl *>(
            ap_command_listener.in());

    DDS::DataReader_var ap_command_dr = subscriber->create_datareader(
        autopilot_command_topic, reader_qos, ap_command_listener,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    // Sensors DataReader
    DDS::DataReaderListener_var sensors_listener(
        new SensorsDataReaderListenerImpl);

    SensorsDataReaderListenerImpl *sensors_listener_servant =
        dynamic_cast<SensorsDataReaderListenerImpl *>(sensors_listener.in());

    DDS::DataReader_var sensors_dr = subscriber->create_datareader(
        sensors_topic, reader_qos, sensors_listener,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    // AIS DataReader
    DDS::DataReaderListener_var aivdm_listener(
        new AivdmMessageDataReaderListenerImpl());
    AivdmMessageDataReaderListenerImpl *aivdm_listener_servant =
        dynamic_cast<AivdmMessageDataReaderListenerImpl *>(aivdm_listener.in());

    DDS::DataReader_var aivdm_dr =
        subscriber->create_datareader(aivdm_topic, reader_qos, aivdm_listener,
                                      OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!route_dr || !ap_command_dr || !loiter_position_dr || !sensors_dr ||
        !aivdm_dr || !mission_dr || !mission_cmd_dr || !proc_act_dr) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - create_datareader failed!\n")),
          1);
    }

    // narrow the readers
    Autopilot::RouteDataReader_var route_dr_i =
        Autopilot::RouteDataReader::_narrow(route_dr);

    Autopilot::AutopilotCommandDataReader_var ap_command_dr_i =
        Autopilot::AutopilotCommandDataReader::_narrow(ap_command_dr);

    Autopilot::LoiterPositionDataReader_var loiter_position_dr_i =
        Autopilot::LoiterPositionDataReader::_narrow(loiter_position_dr);

    Autopilot::DiveProcedureDataReader_var dive_proc_dr_i =
        Autopilot::DiveProcedureDataReader::_narrow(dive_proc_dr);

    Autopilot::MissionDataReader_var mission_dr_i =
        Autopilot::MissionDataReader::_narrow(mission_dr);

    Autopilot::MissionCommandDataReader_var mission_cmd_dr_i =
        Autopilot::MissionCommandDataReader::_narrow(mission_cmd_dr);

    Autopilot::ProcedureActivationDataReader_var proc_act_dr_i =
        Autopilot::ProcedureActivationDataReader::_narrow(proc_act_dr);

    PhysicalState::SensorsDataReader_var sensors_dr_i =
        PhysicalState::SensorsDataReader::_narrow(sensors_dr);

    PhysicalState::AivdmMessageDataReader_var aivdm_dr_i =
        PhysicalState::AivdmMessageDataReader::_narrow(aivdm_dr);

    if (!route_dr_i || !ap_command_dr_i || !loiter_position_dr_i ||
        !dive_proc_dr_i || !sensors_dr_i) {
      ACE_ERROR_RETURN(
          (LM_ERROR, ACE_TEXT("ERROR: %N:%l: main() - _narrow failed!\n")), 1);
    }

    // Block until C2 is available
    DDS::StatusCondition_var condition = ap_command_dr_i->get_statuscondition();
    condition->set_enabled_statuses(DDS::SUBSCRIPTION_MATCHED_STATUS);

    DDS::WaitSet_var ws = new DDS::WaitSet;
    ws->attach_condition(condition);

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Blocking until C2 is available\n")));

    while (true) {
      DDS::SubscriptionMatchedStatus matches;
      if (ap_command_dr_i->get_subscription_matched_status(matches) !=
          DDS::RETCODE_OK) {
        ACE_ERROR_RETURN(
            (LM_ERROR, ACE_TEXT("ERROR: %N:%l: main() - "
                                "get_subscription_matched_status failed!\n")),
            1);
      }
      // subscription topic got matched, C2 is online
      if (matches.current_count >= 1) {
        break;
      }
      // wait for 60 seconds before giving up
      DDS::Duration_t timeout = {60, 0};
      DDS::ConditionSeq conditions;
      if (ws->wait(conditions, timeout) != DDS::RETCODE_OK) {
        ACE_ERROR_RETURN(
            (LM_ERROR,
             ACE_TEXT("ERROR: %N:%l: main() - C2 never connected, exiting\n")),
            1);
      }
    }
    ws->detach_condition(condition);

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("C2 is available \n")));

    // instantiate controllers
    AutopilotController ap_controller = AutopilotController();
    MissionController ms_controller = MissionController(&ap_controller);
    CORBA::Boolean ap_error = false;

    // Main loop, keep listening until C2 disconnects from command topic
    // sleep 250 ms every iteration
    ACE_Time_Value sleep_interval = ACE_Time_Value(0, 250000);
    while (true) {
      ACE_hrtime_t start = ACE_OS::gethrtime();
      DDS::SubscriptionMatchedStatus matches;
      if (ap_command_dr_i->get_subscription_matched_status(matches) !=
          DDS::RETCODE_OK) {
        ACE_ERROR_RETURN(
            (LM_ERROR, ACE_TEXT("ERROR: %N:%l: main() - "
                                "get_subscription_matched_status failed!\n")),
            1);
      }

      // C2 matched before but not currently anymore -> disconnect
      if (matches.current_count == 0 && matches.total_count > 0) {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("C2 disconnected, shutting down\n")));
        break;
      }

      ap_error = false;

      // retrieve the latest procedures and missions
      if (mission_listener_servant->mission_changed()) {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("New mission since last check\n")));
        ms_controller.set_mission(mission_listener_servant->get_mission());
      }

      if (route_listener_servant->new_routes_available()) {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("New route updates since last check\n")));
        std::vector<Autopilot::Route> new_routes =
            route_listener_servant->get_routes();
        for (auto route : new_routes) {
          ap_controller.set_route(route);
        }
      }

      if (loiter_listener_servant->new_positions_available()) {
        ACE_DEBUG((LM_DEBUG,
                   ACE_TEXT("New loiter position updates since last check\n")));
        std::vector<Autopilot::LoiterPosition> new_lps =
            loiter_listener_servant->get_positions();
        for (auto lp : new_lps) {
          ap_controller.set_loiter_position(lp);
        }
      }

      if (aivdm_listener_servant->new_messages_available()) {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("New AIS Messages since last check\n")));
        std::vector<PhysicalState::AivdmMessage> new_ais_msgs =
            aivdm_listener_servant->get_messages();
        ap_controller.update_aivdm(new_ais_msgs);
      }

      if (dive_proc_listener_servant->new_procs_available()) {
        ACE_DEBUG(
            (LM_DEBUG, ACE_TEXT("New dive procs updates since last check\n")));
        std::vector<Autopilot::DiveProcedure> new_dps =
            dive_proc_listener_servant->get_procedures();
        for (auto dp : new_dps) {
          ap_controller.set_dive_procedure(dp);
        }
      }

      // retrieve procedure activation commands
      if (proc_act_listener_servant->new_commands_available()) {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("New proc activations received\n")));
        std::vector<Autopilot::ProcedureActivation> new_acts =
            proc_act_listener_servant->get_latest_commands();
        for (auto act : new_acts) {
          ap_controller.activate_procedure(act, false);
        }
      }

      // retrieve the latest mission and ap commands
      if (mission_cmd_listener_servant->new_commands_available()) {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("New mission commands received\n")));
        std::vector<Autopilot::MissionCommand> new_cmds =
            mission_cmd_listener_servant->get_latest_commands();
        for (auto cmd : new_cmds) {
          ms_controller.execute_command(cmd.command);
        }
      }

      if (ap_command_listener_servant->new_commands_available()) {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("New AP commands received\n")));
        ms_controller.execute_command(Autopilot::MC_SUSPEND);
        std::vector<Autopilot::AutopilotCommand> new_cmds =
            ap_command_listener_servant->get_latest_commands();
        for (auto cmd : new_cmds) {
          ap_controller.update_state(cmd.command, false);
        }
      }

      // retrieve the latest sensor values if they changed
      if (sensors_listener_servant->new_readings_available()) {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("New sensor values received\n")));
        ap_error |= ap_controller.set_sensor_vals(
            sensors_listener_servant->get_readings());
      }

      // run mission controller
      ms_controller.run();

      // run ap controller
      if (!ap_error && ap_controller.execute()) {
        ACE_DEBUG((LM_DEBUG,
                   ACE_TEXT("Actuator output available, writing to topic\n")));
        PhysicalState::Actuators cmds = ap_controller.get_actuator_cmds();
        actuators_dw->write(cmds, DDS::HANDLE_NIL);
      }

      // check if status reports are available to write
      if (ms_controller.is_report_available()) {
        ACE_DEBUG(
            (LM_DEBUG,
             ACE_TEXT("Mission Status Report available, writing to topic\n")));

        ms_report_dw->write(ms_controller.get_report(), DDS::HANDLE_NIL);
      }

      if (ap_controller.is_report_available()) {
        ACE_DEBUG((
            LM_DEBUG,
            ACE_TEXT("Autopilot Status Report available, writing to topic\n")));

        ap_report_dw->write(ap_controller.get_report(), DDS::HANDLE_NIL);
      }

      if (ap_controller.is_colreg_report_available()) {
        ACE_DEBUG((
            LM_DEBUG,
            ACE_TEXT("Autopilot COLREG Report available, writing to topic\n")));
        colreg_status_dw->write(ap_controller.get_colreg_report(),
                                DDS::HANDLE_NIL);
      }

      CORBA::Double delta = (ACE_OS::gethrtime() - start) * 1e-6;
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("Executed AP loop in %f ms, going to sleep\n"),
                 delta));
      ACE_OS::sleep(sleep_interval);
    }

    // Cleanup
    participant->delete_contained_entities();
    dpf->delete_participant(participant);

    TheServiceParticipant->shutdown();
  } catch (const CORBA::Exception &e) {
    e._tao_print_exception("Exception caught in main():");
    return 1;
  }
}
