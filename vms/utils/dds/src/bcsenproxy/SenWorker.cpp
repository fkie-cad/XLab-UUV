#include "SenWorker.h"

#include <ace/Guard_T.h>

#include <boost/archive/archive_exception.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>

#include "../BcProxyMessages.h"

void* sen_worker(void* args) {
  SenWorkerArgs* arguments = reinterpret_cast<SenWorkerArgs*>(args);
  boost::asio::io_context io_context;
  boost::asio::ip::udp::socket receive_socket(io_context);
  receive_socket.open(boost::asio::ip::udp::v4());
  receive_socket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(),
                                                     arguments->rcv_port));

  while (true) {
    // set up buffer to read data to
    std::string rcv_buffer;
    rcv_buffer.resize(2048);
    // blocking read
    auto nread = receive_socket.receive(boost::asio::buffer(rcv_buffer));
    if (nread == 0) continue;

    try {
      // deserialization
      std::istringstream archive_stream(rcv_buffer);
      SensorReport bc_sensor_report;
      boost::archive::text_iarchive archive(archive_stream);
      archive >> bc_sensor_report;

      // build DDS sensor report
      PhysicalState::Sensors proxied_report;
      proxied_report.bc_id = 123;
      proxied_report.course_over_ground = bc_sensor_report.course_over_ground;

      proxied_report.gnss_1 = PhysicalState::Coordinates(
          {bc_sensor_report.gnss_1[0], bc_sensor_report.gnss_1[1]});
      proxied_report.gnss_2 = PhysicalState::Coordinates(
          {bc_sensor_report.gnss_2[0], bc_sensor_report.gnss_2[1]});
      proxied_report.gnss_3 = PhysicalState::Coordinates(
          {bc_sensor_report.gnss_3[0], bc_sensor_report.gnss_3[1]});

      proxied_report.heading = bc_sensor_report.heading;
      proxied_report.rate_of_turn = bc_sensor_report.rate_of_turn;
      proxied_report.rpm_port = bc_sensor_report.rpm_port;
      proxied_report.rpm_stbd = bc_sensor_report.rpm_stbd;
      proxied_report.rudder_angle = bc_sensor_report.rudder_angle;
      proxied_report.speed = bc_sensor_report.speed;
      proxied_report.speed_over_ground = bc_sensor_report.speed_over_ground;
      proxied_report.ship_depth = bc_sensor_report.ship_depth;
      proxied_report.depth_under_keel = bc_sensor_report.depth_under_keel;
      proxied_report.throttle_port = bc_sensor_report.throttle_port;
      proxied_report.throttle_stbd = bc_sensor_report.throttle_stbd;
      proxied_report.buoyancy = bc_sensor_report.buoyancy;

      ACE_DEBUG(
          (LM_DEBUG,
           ACE_TEXT("Writing proxied sensor report\n"
                    "    cog:           %f\n"
                    "    gnss_1:        %f, %f\n"
                    "    gnss_2:        %f, %f\n"
                    "    gnss_3:        %f, %f\n"
                    "    heading:       %f\n"
                    "    rot:           %f\n"
                    "    rpm_port:      %f\n"
                    "    rpm_stbd:      %f\n"
                    "    rudder:        %f\n"
                    "    speed:         %f\n"
                    "    sog:           %f\n"
                    "    throttle_port: %f\n"
                    "    throttle_stbd: %f\n"),
           proxied_report.course_over_ground, proxied_report.gnss_1.latitude,
           proxied_report.gnss_1.longitude, proxied_report.gnss_2.latitude,
           proxied_report.gnss_2.longitude, proxied_report.gnss_3.latitude,
           proxied_report.gnss_3.longitude, proxied_report.heading,
           proxied_report.rate_of_turn, proxied_report.rpm_port,
           proxied_report.rpm_stbd, proxied_report.rudder_angle,
           proxied_report.speed, proxied_report.speed_over_ground,
           proxied_report.throttle_port, proxied_report.throttle_stbd));
      
      DDS::ReturnCode_t s_error =
        arguments->sensors_dw->write(proxied_report, DDS::HANDLE_NIL);
    } catch (boost::archive::archive_exception& e) {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("Archive exception while trying to deserialize "
                          "Sensor report: %s\n"),
                 (const char*)e.what()));
    }
  }
}
