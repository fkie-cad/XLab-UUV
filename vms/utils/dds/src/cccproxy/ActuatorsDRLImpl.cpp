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

#include "./messages.pb.h"
#include "PhysicalStateTypeSupportImpl.h"

ActuatorsDataReaderListenerImpl::ActuatorsDataReaderListenerImpl(
    std::string ccc_snd_addr, std::string ccc_snd_port) {
  // set up asio send socket
  boost::asio::ip::udp::resolver resolver(send_service_);
  boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(),
                                              ccc_snd_addr, ccc_snd_port);
  this->ccc_rcv_endpoint_ = *resolver.resolve(query);
  send_socket_ = new boost::asio::ip::udp::socket(send_service_);
}

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

  // proxy for AP, forward message to CCC
  if (error == DDS::RETCODE_OK) {
    if (info.valid_data) {
      // update protobuf
      TelemetryReport telemetry_report{};
      ActuatorCmdReport* report_pb = telemetry_report.mutable_act_cmd_report();
      report_pb->set_rudder_angle(actuators.rudder_angle);
      report_pb->set_engine_throttle_port(actuators.engine_throttle_port);
      report_pb->set_engine_throttle_stbd(actuators.engine_throttle_stbd);
      report_pb->set_ballast_tank_pump(actuators.ballast_tank_pump);
      report_pb->set_thruster_throttle_bow(actuators.thruster_throttle_bow);
      report_pb->set_thruster_throttle_stern(
          actuators.thruster_throttle_stern);

      // serialize and dump via UDP
      if (!send_socket_->is_open())
        send_socket_->open(boost::asio::ip::udp::v4());

      send_socket_->send_to(
          boost::asio::buffer(telemetry_report.SerializeAsString()),
          this->ccc_rcv_endpoint_);
    }
  } else {
    ACE_ERROR((LM_ERROR, ACE_TEXT("ERROR: %N:%l: on_data_available() - "
                                  "take_next_sample failed!\n")));
  }
}
