#include <ace/Log_Msg.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/StaticIncludes.h>
#include <dds/DCPS/WaitSet.h>
#include <dds/DdsDcpsCoreC.h>
#include <dds/DdsDcpsInfrastructureC.h>
#include <dds/DdsDcpsPublicationC.h>

#include <boost/asio/buffer.hpp>
#include <string>
#ifdef ACE_AS_STATIC_LIBS
#endif

#include <boost/archive/text_iarchive.hpp>

#include "ActuatorsDRLImpl.h"
#include "PhysicalStateTypeSupportImpl.h"

int missing_arg(std::string arg) {
  ACE_ERROR_RETURN((LM_ERROR,
                    ACE_TEXT("ERROR: %N:%l: main() - missing "
                             "value for argument %s!\n"),
                    arg.c_str()),
                   1);
}

int ACE_TMAIN(int argc, ACE_TCHAR *argv[]) {
  // parse args to find the address/port we should use for the ASIO setup
  std::string bc_snd_addr = "localhost";
  std::string bc_snd_port = "10113";
  for (int i = 0; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-bc-snd-addr") {
      if (i == argc - 1) return missing_arg(arg);
      bc_snd_addr = argv[i + 1];
    } else if (arg == "-bc-snd-port") {
      if (i == argc - 1) return missing_arg(arg);
      bc_snd_port = argv[i + 1];
    }
  }
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("Parsed args: send to BC on %s:%s\n"),
             bc_snd_addr.c_str(), bc_snd_port.c_str()));
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

    // Register type support for PhysicalState::Actuators
    PhysicalState::ActuatorsTypeSupport_var actuators_servant =
        new PhysicalState::ActuatorsTypeSupportImpl();
    if (actuators_servant->register_type(participant, "") != DDS::RETCODE_OK) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - register_type failed!\n")),
          1);
    }

    // Create the topic for the Actuators
    CORBA::String_var actuators_type_name = actuators_servant->get_type_name();
    DDS::Topic_var actuators_topic = participant->create_topic(
        "BC Actuators", actuators_type_name, TOPIC_QOS_DEFAULT, 0,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!actuators_topic) {
      ACE_ERROR_RETURN(
          (LM_ERROR, ACE_TEXT("ERROR: %N:%l: main() - create_topic failed!\n")),
          1);
    }

    // Create Subscriber for the Actuators topic
    DDS::Subscriber_var subscriber = participant->create_subscriber(
        SUBSCRIBER_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!subscriber) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - create_subscriber failed!\n")),
          1);
    }

    // Create the DataReader for the Actuators topic
    // Create the listener
    DDS::DataReaderListener_var actuators_listener(
        new ActuatorsDataReaderListenerImpl(bc_snd_addr, bc_snd_port));

    DDS::DataReaderQos reader_qos;
    subscriber->get_default_datareader_qos(reader_qos);
    reader_qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;

    // Create the reader
    DDS::DataReader_var actuators_dr = subscriber->create_datareader(
        actuators_topic, reader_qos, actuators_listener,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!actuators_dr) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - create_datareader failed!\n")),
          1);
    }

    PhysicalState::ActuatorsDataReader_var actuators_dr_i =
        PhysicalState::ActuatorsDataReader::_narrow(actuators_dr);

    if (!actuators_dr) {
      ACE_ERROR_RETURN(
          (LM_ERROR, ACE_TEXT("ERROR: %N:%l: main() - _narrow failed!\n")), 1);
    }

    // Wait for Autopilot to be available
    DDS::StatusCondition_var condition = actuators_dr->get_statuscondition();
    condition->set_enabled_statuses(DDS::SUBSCRIPTION_MATCHED_STATUS);

    DDS::WaitSet_var ws = new DDS::WaitSet;
    ws->attach_condition(condition);

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Blocking until Autopilot is available\n")));

    while (true) {
      DDS::SubscriptionMatchedStatus matches;
      if (actuators_dr->get_subscription_matched_status(matches) !=
          DDS::RETCODE_OK) {
        ACE_ERROR_RETURN(
            (LM_ERROR, ACE_TEXT("ERROR: %N:%l: main() - "
                                "get_subscription_matched_status failed!\n")),
            1);
      }
      // subscription topic got matched, autopilot is online
      if (matches.current_count >= 1) {
        break;
      }
      // wait for 60 seconds before giving up
      DDS::Duration_t timeout = {60, 0};
      DDS::ConditionSeq conditions;
      if (ws->wait(conditions, timeout) != DDS::RETCODE_OK) {
        ACE_ERROR_RETURN((LM_ERROR, ACE_TEXT("ERROR: %N:%l: main() - Autopilot "
                                             "never connected, exiting\n")),
                         1);
      }
    }
    ws->detach_condition(condition);

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Autopilot is available \n")));

    // Loop until publisher is done
    ACE_Time_Value sleep_interval = ACE_Time_Value(1, 0);
    while (true) {
      DDS::SubscriptionMatchedStatus matches;
      if (actuators_dr->get_subscription_matched_status(matches) !=
          DDS::RETCODE_OK) {
        ACE_ERROR_RETURN(
            (LM_ERROR, ACE_TEXT("ERROR: %N:%l: main() - "
                                "get_subscription_matched_status failed!\n")),
            1);
      }

      // stop loop if publisher (Autopilot) disconnected
      if (matches.current_count == 0 && matches.total_count > 0) {
        ACE_DEBUG(
            (LM_DEBUG, ACE_TEXT("Autopilot disconnected, shutting down\n")));
        break;
      }

      // sleep 1 second
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
