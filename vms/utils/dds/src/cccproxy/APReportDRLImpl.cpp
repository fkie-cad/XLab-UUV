#include "APReportDRLImpl.h"

#include <ace/Guard_T.h>
#include <ace/Log_Msg.h>
#include <ace/OS_NS_stdlib.h>

#include <iostream>

#include "./messages.pb.h"
#include "AutopilotC.h"
#include "AutopilotTypeSupportC.h"
#include "AutopilotTypeSupportImpl.h"

APReportDataReaderListenerImpl::APReportDataReaderListenerImpl(
    std::string ccc_snd_addr, std::string ccc_snd_port) {
  // set up asio send socket
  boost::asio::ip::udp::resolver resolver(send_service_);
  boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(),
                                              ccc_snd_addr, ccc_snd_port);
  this->ccc_rcv_endpoint_ = *resolver.resolve(query);
  send_socket_ = new boost::asio::ip::udp::socket(send_service_);
}

void APReportDataReaderListenerImpl::on_requested_deadline_missed(
    DDS::DataReader_ptr reader,
    const DDS::RequestedDeadlineMissedStatus &status) {}

void APReportDataReaderListenerImpl::on_requested_incompatible_qos(
    DDS::DataReader_ptr reader,
    const DDS::RequestedIncompatibleQosStatus &status) {}

void APReportDataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr reader, const DDS::SampleRejectedStatus &status) {}

void APReportDataReaderListenerImpl::on_liveliness_changed(
    DDS::DataReader_ptr reader, const DDS::LivelinessChangedStatus &status) {}

void APReportDataReaderListenerImpl::on_subscription_matched(
    DDS::DataReader_ptr reader, const DDS::SubscriptionMatchedStatus &status) {}

void APReportDataReaderListenerImpl::on_sample_lost(
    DDS::DataReader_ptr reader, const DDS::SampleLostStatus &status) {}

void APReportDataReaderListenerImpl::on_data_available(
    DDS::DataReader_ptr reader) {
  Autopilot::APReportDataReader_var reader_i =
      Autopilot::APReportDataReader::_narrow(reader);

  if (!reader_i) {
    ACE_ERROR(
        (LM_ERROR,
         ACE_TEXT("ERROR: %N:%l: on_data_available() - _narrow failed!\n")));
    ACE_OS::exit(1);
  }

  Autopilot::APReport report;
  DDS::SampleInfo info;

  const DDS::ReturnCode_t error = reader_i->take_next_sample(report, info);

  // proxy for AP, forward message to CCC
  if (error == DDS::RETCODE_OK) {
    if (info.valid_data) {
      // update protobuf
      TelemetryReport telemetry_report{};
      APReport *report_pb = telemetry_report.mutable_ap_report();
      report_pb->mutable_active_waypoint()->mutable_coords()->set_latitude(
          report.active_waypoint.coords.latitude);
      report_pb->mutable_active_waypoint()->mutable_coords()->set_longitude(
          report.active_waypoint.coords.longitude);

      // WARNING: we're relying on the 1:1 match between enum values here
      report_pb->set_state(AutopilotState(report.state));

      report_pb->set_active_route_id(report.active_route_id);
      report_pb->set_route_progress(report.route_progress);
      report_pb->set_route_len(report.route_len);
      report_pb->set_route_name(report.route_name);
      report_pb->set_tgt_speed(report.tgt_speed);
      report_pb->set_active_lp_id(report.active_lp_id);
      report_pb->set_lp_dist(report.lp_dist);
      report_pb->set_lp_name(report.lp_name);
      report_pb->set_dp_name(report.dp_name);
      report_pb->set_tgt_depth(report.tgt_depth);
      report_pb->mutable_gnss_ap()->set_latitude(report.gnss_ap.latitude);
      report_pb->mutable_gnss_ap()->set_longitude(report.gnss_ap.longitude);

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
