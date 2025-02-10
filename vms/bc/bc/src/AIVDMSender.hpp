#ifndef __AIVDM_SENDER_HPP_INCLUDED__
#define __AIVDM_SENDER_HPP_INCLUDED__

#include <string>
#include <vector>

#include "IrrlichtDevice.h"
#include "SimulationModel.hpp"
#include "libs/asio/include/asio/io_service.hpp"
#include "libs/asio/include/asio/ip/udp.hpp"

class AivdmSender {
 public:
  AivdmSender(SimulationModel* model, irr::IrrlichtDevice* dev,
              std::string snd_address, std::string snd_port);
  void send_aivdm();

 private:
  std::string add_nmea_checksum(std::string);

  asio::io_service io_service;
  asio::ip::udp::endpoint receiver_endpoint;
  asio::ip::udp::socket* snd_socket;

  irr::IrrlichtDevice* device;
  SimulationModel* model;

  std::vector<std::string> messages;
};

#endif
