#ifndef __NETWORK_CONTROLLER_HPP_INCLUDED__
#define __NETWORK_CONTROLLER_HPP_INCLUDED__

#include <mutex>
#include <string>

#include "BcProxyMessages.hpp"
#include "IrrlichtDevice.h"
#include "SimulationModel.hpp"
#include "libs/asio/include/asio/io_service.hpp"
#include "libs/asio/include/asio/ip/udp.hpp"

class NetworkController {
 public:
  NetworkController(SimulationModel* model, irr::IrrlichtDevice* dev,
                    std::string snd_address, std::string snd_port,
                    std::string rcv_port);
  ~NetworkController();
  void send_report();
  void update_model();
  void receive_loop(std::string rcv_port);

 private:
  asio::io_service io_service;
  asio::ip::udp::endpoint receiver_endpoint;
  asio::ip::udp::socket* snd_socket;

  irr::IrrlichtDevice* device;
  SimulationModel* model;
  irr::u32 last_send;
  static const irr::u32 SEND_INTERVAL = 200;
  std::mutex terminate_rcv_mutex;
  bool terminate_rcv_thread;
  std::mutex actuator_cmd_mutex;
  ActuatorCommands actuator_cmd;
  bool fresh_cmd;
};

#endif
