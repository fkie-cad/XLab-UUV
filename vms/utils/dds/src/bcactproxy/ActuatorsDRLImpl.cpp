#include "ActuatorsDRLImpl.h"

#include <ace/Guard_T.h>
#include <ace/Log_Msg.h>
#include <ace/Mutex.h>
#include <ace/OS_NS_stdlib.h>
#include <tao/Basic_Types.h>

#include <boost/archive/text_oarchive.hpp>
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <iostream>

#include "../BcProxyMessages.h"
#include "PhysicalStateTypeSupportImpl.h"


ActuatorsDataReaderListenerImpl::ActuatorsDataReaderListenerImpl(
    std::string bc_snd_addr, std::string bc_snd_port) {
  // set up asio send socket
  boost::asio::ip::udp::resolver resolver(send_service);
  boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(),
                                              bc_snd_addr, bc_snd_port);
  bc_rcv_endpoint = *resolver.resolve(query);
  send_socket = new boost::asio::ip::udp::socket(send_service);
}

ActuatorsDataReaderListenerImpl::~ActuatorsDataReaderListenerImpl() {}

void ActuatorsDataReaderListenerImpl::on_requested_deadline_missed(
    DDS::DataReader_ptr reader,
    const DDS::RequestedDeadlineMissedStatus &status) {}

void ActuatorsDataReaderListenerImpl::on_requested_incompatible_qos(
    DDS::DataReader_ptr reader,
    const DDS::RequestedIncompatibleQosStatus &status) {}

void ActuatorsDataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr reader, const DDS::SampleRejectedStatus &status) {}

void ActuatorsDataReaderListenerImpl::on_liveliness_changed(
    DDS::DataReader_ptr reader, const DDS::LivelinessChangedStatus &status) {}

void ActuatorsDataReaderListenerImpl::on_subscription_matched(
    DDS::DataReader_ptr reader, const DDS::SubscriptionMatchedStatus &status) {}

void ActuatorsDataReaderListenerImpl::on_sample_lost(
    DDS::DataReader_ptr reader, const DDS::SampleLostStatus &status) {}

void ActuatorsDataReaderListenerImpl::on_data_available(
    DDS::DataReader_ptr reader) {
  PhysicalState::ActuatorsDataReader_var reader_i =
      PhysicalState::ActuatorsDataReader::_narrow(reader);

  if (!reader_i) {
    ACE_ERROR(
        (LM_ERROR,
         ACE_TEXT("ERROR: %N:%l: on_data_available() - _narrow failed!\n")));
    ACE_OS::exit(1);
  }

  PhysicalState::Actuators actuators;
  DDS::SampleInfo info;

  const DDS::ReturnCode_t error = reader_i->take_next_sample(actuators, info);

  ACE_Guard<ACE_Mutex> guard(this->lock_);
  // proxy for BC, forward message to BC
  if (error == DDS::RETCODE_OK) {
    if (info.valid_data) {
      // build struct
      
          ActuatorCommands commands;
          commands.rudder_angle = actuators.rudder_angle;
          commands.engine_throttle_port = actuators.engine_throttle_port;
          commands.engine_throttle_stbd = actuators.engine_throttle_stbd;
          commands.ballast_tank_pump = actuators.ballast_tank_pump;
          commands.thruster_throttle_bow = actuators.thruster_throttle_bow;
          commands.thruster_throttle_stern = actuators.thruster_throttle_stern;
          // serialize
          std::ostringstream archive_stream;
          boost::archive::text_oarchive archive(archive_stream);
          archive << commands;
          // send
          if (!send_socket->is_open())
            send_socket->open(boost::asio::ip::udp::v4());

          send_socket->send_to(boost::asio::buffer(archive_stream.str()),
                                bc_rcv_endpoint);
    }
  } else {
    ACE_ERROR((LM_ERROR, ACE_TEXT("ERROR: %N:%l: on_data_available() - "
                                  "take_next_sample failed!\n")));
  }
}
