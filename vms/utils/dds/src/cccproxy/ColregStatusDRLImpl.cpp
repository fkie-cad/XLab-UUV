#include "ColregStatusDRLImpl.h"

#include "./messages.pb.h"
#include "AutopilotTypeSupportC.h"

ColregStatusDataReaderListenerImpl::ColregStatusDataReaderListenerImpl(
    std::string ccc_snd_addr, std::string ccc_snd_port) {
  // set up asio send socket
  boost::asio::ip::udp::resolver resolver(send_service_);
  boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(),
                                              ccc_snd_addr, ccc_snd_port);
  this->ccc_rcv_endpoint_ = *resolver.resolve(query);
  send_socket_ = new boost::asio::ip::udp::socket(send_service_);
}

void ColregStatusDataReaderListenerImpl::on_requested_deadline_missed(
    DDS::DataReader_ptr reader,
    const DDS::RequestedDeadlineMissedStatus &status) {}

void ColregStatusDataReaderListenerImpl::on_requested_incompatible_qos(
    DDS::DataReader_ptr reader,
    const DDS::RequestedIncompatibleQosStatus &status) {}

void ColregStatusDataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr reader, const DDS::SampleRejectedStatus &status) {}

void ColregStatusDataReaderListenerImpl::on_liveliness_changed(
    DDS::DataReader_ptr reader, const DDS::LivelinessChangedStatus &status) {}

void ColregStatusDataReaderListenerImpl::on_subscription_matched(
    DDS::DataReader_ptr reader, const DDS::SubscriptionMatchedStatus &status) {}

void ColregStatusDataReaderListenerImpl::on_sample_lost(
    DDS::DataReader_ptr reader, const DDS::SampleLostStatus &status) {}

void ColregStatusDataReaderListenerImpl::on_data_available(
    DDS::DataReader_ptr reader) {
  Autopilot::ColregStatusDataReader_var reader_i =
      Autopilot::ColregStatusDataReader::_narrow(reader);

  if (!reader_i) {
    ACE_ERROR(
        (LM_ERROR,
         ACE_TEXT("ERROR: %N:%l: on_data_available() - _narrow failed!\n")));
    ACE_OS::exit(1);
  }

  Autopilot::ColregStatus report;
  DDS::SampleInfo info;

  const DDS::ReturnCode_t error = reader_i->take_next_sample(report, info);

  // proxy for AP, forward message to CCC
  if (error == DDS::RETCODE_OK) {
    if (info.valid_data) {
      // update protobuf
      TelemetryReport telemetry_report{};
      ColregStatus *report_pb = telemetry_report.mutable_colreg_report();
      report_pb->mutable_tgt_pos()->set_latitude(report.tgt_pos.latitude);
      report_pb->mutable_tgt_pos()->set_longitude(report.tgt_pos.longitude);
      report_pb->set_tgt_mmsi(report.tgt_mmsi);

      // WARNING: we're relying on the 1:1 match between enum values here
      report_pb->set_type(ColregType(report.type));

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
