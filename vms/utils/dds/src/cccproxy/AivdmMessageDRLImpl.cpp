#include "AivdmMessageDRLImpl.h"

#include <ace/Guard_T.h>
#include <ace/Log_Msg.h>
#include <ace/OS_NS_stdlib.h>

#include <iostream>

#include "./messages.pb.h"
#include "PhysicalStateC.h"
#include "PhysicalStateTypeSupportC.h"
#include "PhysicalStateTypeSupportImpl.h"

AivdmMessageDataReaderListenerImpl::AivdmMessageDataReaderListenerImpl(
    std::string ccc_snd_addr, std::string ccc_snd_port) {
  // set up asio send socket
  boost::asio::ip::udp::resolver resolver(send_service_);
  boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(),
                                              ccc_snd_addr, ccc_snd_port);
  this->ccc_rcv_endpoint_ = *resolver.resolve(query);
  send_socket_ = new boost::asio::ip::udp::socket(send_service_);
}

void AivdmMessageDataReaderListenerImpl::on_requested_deadline_missed(
    DDS::DataReader_ptr reader,
    const DDS::RequestedDeadlineMissedStatus &status) {}

void AivdmMessageDataReaderListenerImpl::on_requested_incompatible_qos(
    DDS::DataReader_ptr reader,
    const DDS::RequestedIncompatibleQosStatus &status) {}

void AivdmMessageDataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr reader, const DDS::SampleRejectedStatus &status) {}

void AivdmMessageDataReaderListenerImpl::on_liveliness_changed(
    DDS::DataReader_ptr reader, const DDS::LivelinessChangedStatus &status) {}

void AivdmMessageDataReaderListenerImpl::on_subscription_matched(
    DDS::DataReader_ptr reader, const DDS::SubscriptionMatchedStatus &status) {}

void AivdmMessageDataReaderListenerImpl::on_sample_lost(
    DDS::DataReader_ptr reader, const DDS::SampleLostStatus &status) {}

void AivdmMessageDataReaderListenerImpl::on_data_available(
    DDS::DataReader_ptr reader) {
  PhysicalState::AivdmMessageDataReader_var reader_i =
      PhysicalState::AivdmMessageDataReader::_narrow(reader);

  if (!reader_i) {
    ACE_ERROR(
        (LM_ERROR,
         ACE_TEXT("ERROR: %N:%l: on_data_available() - _narrow failed!\n")));
    ACE_OS::exit(1);
  }

  PhysicalState::AivdmMessage ais_message;
  DDS::SampleInfo info;

  const DDS::ReturnCode_t error = reader_i->take_next_sample(ais_message, info);

  // proxy for BC, forward message to CCC
  if (error == DDS::RETCODE_OK) {
    if (info.valid_data) {
      // update protobuf
      TelemetryReport telemetry_report{};
      WrappedNmea *report_pb = telemetry_report.mutable_nmea_report();
      report_pb->set_msg(ais_message.message);

      // serialize and dump via UDP
      if (!send_socket_->is_open())
        send_socket_->open(boost::asio::ip::udp::v4());

      send_socket_->send_to(
          boost::asio::buffer(telemetry_report.SerializeAsString()),
          this->ccc_rcv_endpoint_);
    }
  } else {
    ACE_ERROR((
        LM_ERROR,
        ACE_TEXT(
            "ERROR: %N:%l: on_data_available() - take_next_sample failed!\n")));
  }
}
