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
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>

#include "AisWorker.h"
#include "PhysicalStateTypeSupportImpl.h"
#include "SenWorker.h"

int missing_arg(std::string arg) {
  ACE_ERROR_RETURN((LM_ERROR,
                    ACE_TEXT("ERROR: %N:%l: main() - missing "
                             "value for argument %s!\n"),
                    arg.c_str()),
                   1);
}

int ACE_TMAIN(int argc, ACE_TCHAR *argv[]) {
  // parse args to find the address/port we should use for the ASIO setup
  std::string sensor_port = "10112";
  std::string ais_port = "10114";

  for (int i = 0; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-ais-port") {
      if (i == argc - 1) return missing_arg(arg);
      ais_port = argv[i + 1];
    } else if (arg == "-sensor-port") {
      if (i == argc - 1) return missing_arg(arg);
      sensor_port = argv[i + 1];
    }
  }
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("Parsed args: listen to BC sensors on 0.0.0.0:%s, AIS "
                      "reports on 0.0.0.0:%s\n"),
             sensor_port.c_str(), ais_port.c_str()));
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

    // Register type support for Sensors and AIS messages
    PhysicalState::SensorsTypeSupport_var sensors_servant =
        new PhysicalState::SensorsTypeSupportImpl();
    PhysicalState::AivdmMessageTypeSupport_var aivdm_servant =
        new PhysicalState::AivdmMessageTypeSupportImpl();

    if (sensors_servant->register_type(participant, "") != DDS::RETCODE_OK ||
        aivdm_servant->register_type(participant, "") != DDS::RETCODE_OK) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - register_type failed!\n")),
          1);
    }

    // Create the topics
    CORBA::String_var sensors_type_name = sensors_servant->get_type_name();
    CORBA::String_var aivdm_type_name = aivdm_servant->get_type_name();

    DDS::Topic_var sensors_topic = participant->create_topic(
        "BC Sensors", sensors_type_name, TOPIC_QOS_DEFAULT, 0,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    DDS::Topic_var aivdm_topic = participant->create_topic(
        "AIS Messages", aivdm_type_name, TOPIC_QOS_DEFAULT, 0,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!sensors_topic || !aivdm_topic) {
      ACE_ERROR_RETURN(
          (LM_ERROR, ACE_TEXT("ERROR: %N:%l: main() - create_topic failed!\n")),
          1);
    }

    // Create the publisher for the Sensors and AIS topics
    DDS::Publisher_var publisher = participant->create_publisher(
        PUBLISHER_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!publisher) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - create_publisher failed!\n")),
          1);
    }

    // Create the DataWriter for the Sensors and AIS topic
    DDS::DataWriter_var sensors_base_dw =
        publisher->create_datawriter(sensors_topic, DATAWRITER_QOS_DEFAULT, 0,
                                     OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    DDS::DataWriter_var aivdm_base_dw =
        publisher->create_datawriter(aivdm_topic, DATAWRITER_QOS_DEFAULT, 0,
                                     OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!sensors_base_dw || !aivdm_base_dw) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - create_datawriter failed!\n")),
          1);
    }

    PhysicalState::SensorsDataWriter_var sensors_dw =
        PhysicalState::SensorsDataWriter::_narrow(sensors_base_dw);

    PhysicalState::AivdmMessageDataWriter_var aivdm_dw =
        PhysicalState::AivdmMessageDataWriter::_narrow(aivdm_base_dw);

    if (!sensors_dw || !aivdm_dw) {
      ACE_ERROR_RETURN(
          (LM_ERROR,
           ACE_TEXT("ERROR: %N:%l: main() - _narrow(writer) failed!\n")),
          1);
    }

    // set up workers for sen and ais forwarding
    SenWorkerArgs sen_worker_args =
        SenWorkerArgs({sensors_dw, std::stoi(sensor_port)});

    AisWorkerArgs ais_worker_args =
        AisWorkerArgs({aivdm_dw, std::stoi(ais_port)});
    // spawn threads
    ACE_Thread::spawn((ACE_THR_FUNC)sen_worker, &sen_worker_args);
    ACE_Thread::spawn((ACE_THR_FUNC)ais_worker, &ais_worker_args);

    // Loop until killed
    ACE_Time_Value sleep_interval = ACE_Time_Value(1, 0);
    while (true) {
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
