#include "SensorsDRLImpl.h"

#include <ace/Guard_T.h>
#include <ace/Log_Msg.h>
#include <ace/OS_NS_stdlib.h>

#include <iostream>

#include "./messages.pb.h"
#include "PhysicalStateC.h"
#include "PhysicalStateTypeSupportC.h"
#include "PhysicalStateTypeSupportImpl.h"

SensorsDataReaderListenerImpl::SensorsDataReaderListenerImpl(
    std::string ccc_snd_addr, std::string ccc_snd_port) {
  // set up asio send socket
  boost::asio::ip::udp::resolver resolver(send_service_);
  boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(),
                                              ccc_snd_addr, ccc_snd_port);
  this->ccc_rcv_endpoint_ = *resolver.resolve(query);
  send_socket_ = new boost::asio::ip::udp::socket(send_service_);
}

void SensorsDataReaderListenerImpl::on_requested_deadline_missed(
    DDS::DataReader_ptr reader,
    const DDS::RequestedDeadlineMissedStatus &status) {}

void SensorsDataReaderListenerImpl::on_requested_incompatible_qos(
    DDS::DataReader_ptr reader,
    const DDS::RequestedIncompatibleQosStatus &status) {}

void SensorsDataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr reader, const DDS::SampleRejectedStatus &status) {}

void SensorsDataReaderListenerImpl::on_liveliness_changed(
    DDS::DataReader_ptr reader, const DDS::LivelinessChangedStatus &status) {}

void SensorsDataReaderListenerImpl::on_subscription_matched(
    DDS::DataReader_ptr reader, const DDS::SubscriptionMatchedStatus &status) {}

void SensorsDataReaderListenerImpl::on_sample_lost(
    DDS::DataReader_ptr reader, const DDS::SampleLostStatus &status) {}

void SensorsDataReaderListenerImpl::on_data_available(
    DDS::DataReader_ptr reader) {
  PhysicalState::SensorsDataReader_var reader_i =
      PhysicalState::SensorsDataReader::_narrow(reader);

  if (!reader_i) {
    ACE_ERROR(
        (LM_ERROR,
         ACE_TEXT("ERROR: %N:%l: on_data_available() - _narrow failed!\n")));
    ACE_OS::exit(1);
  }

  PhysicalState::Sensors readings;
  DDS::SampleInfo info;

  const DDS::ReturnCode_t error = reader_i->take_next_sample(readings, info);

  // proxy for BC, forward message to CCC
  if (error == DDS::RETCODE_OK) {
    if (info.valid_data) {
      // update protobuf
      TelemetryReport telemetry_report{};
      SensorReport *report_pb = telemetry_report.mutable_sensor_report();
      report_pb->mutable_gnss_1()->set_latitude(readings.gnss_1.latitude);
      report_pb->mutable_gnss_1()->set_longitude(readings.gnss_1.longitude);
      report_pb->mutable_gnss_2()->set_latitude(readings.gnss_2.latitude);
      report_pb->mutable_gnss_2()->set_longitude(readings.gnss_2.longitude);
      report_pb->mutable_gnss_3()->set_latitude(readings.gnss_3.latitude);
      report_pb->mutable_gnss_3()->set_longitude(readings.gnss_3.longitude);

      report_pb->set_cog(readings.course_over_ground);
      report_pb->set_heading(readings.heading);
      report_pb->set_rot(readings.rate_of_turn);
      report_pb->set_rpm_port(readings.rpm_port);
      report_pb->set_rpm_stbd(readings.rpm_stbd);
      report_pb->set_rudder_angle(readings.rudder_angle);
      report_pb->set_sog(readings.speed_over_ground);
      report_pb->set_speed(readings.speed);
      report_pb->set_depth_under_keel(readings.depth_under_keel);
      report_pb->set_ship_depth(readings.ship_depth);
      report_pb->set_throttle_port(readings.throttle_port);
      report_pb->set_throttle_stbd(readings.throttle_stbd);
      report_pb->set_buoyancy(readings.buoyancy);

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
