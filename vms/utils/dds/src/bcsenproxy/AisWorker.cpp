#include "AisWorker.h"

#include <ace/Guard_T.h>

#include <boost/archive/archive_exception.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <iostream>
#include <sstream>

#include "../BcProxyMessages.h"


void* ais_worker(void* args) {
  AisWorkerArgs* arguments = reinterpret_cast<AisWorkerArgs*>(args);
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
      AivdmMessage aivdm_msg;
      boost::archive::text_iarchive archive(archive_stream);
      archive >> aivdm_msg;

      ACE_DEBUG(
          (LM_DEBUG, ACE_TEXT("Received AIS for mmsi %i\n"), aivdm_msg.mmsi));

      // build DDS AIS wrapper
      PhysicalState::AivdmMessage proxied_message;
      proxied_message.message = aivdm_msg.message.c_str();
      proxied_message.message_type = aivdm_msg.message_type;
      proxied_message.mmsi = aivdm_msg.mmsi;
      if (aivdm_msg.navigation_status <= 15) {
        proxied_message.navigation_status =
            PhysicalState::NavigationStatus(aivdm_msg.navigation_status);
      } else {
        proxied_message.navigation_status =
            PhysicalState::NavigationStatus::NOT_DEFINED;
      }
      proxied_message.latitude = aivdm_msg.latitude;
      proxied_message.longitude = aivdm_msg.longitude;
      proxied_message.rate_of_turn = aivdm_msg.rate_of_turn;
      proxied_message.speed_over_ground = aivdm_msg.speed_over_ground;
      proxied_message.course_over_ground = aivdm_msg.course_over_ground;
      proxied_message.true_heading = aivdm_msg.true_heading;

      DDS::ReturnCode_t s_error =
        arguments->aivdm_dw->write(proxied_message, DDS::HANDLE_NIL);
    } catch (boost::archive::archive_exception& e) {
      ACE_ERROR((
          LM_ERROR,
          ACE_TEXT(
              "Archive exception while trying to deserialize AIS report: %s\n"),
          (const char*)e.what()));
    }
  }
}
