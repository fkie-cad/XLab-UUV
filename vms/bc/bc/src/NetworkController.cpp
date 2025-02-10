#include "./NetworkController.hpp"

#include <boost/archive/archive_exception.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <iostream>
#include <string>
#include <thread>

#include "./BcProxyMessages.hpp"
#include "./SimulationModel.hpp"
#include "IrrlichtDevice.h"
#include "irrTypes.h"
#include "libs/asio/include/asio/ip/udp.hpp"

NetworkController::NetworkController(SimulationModel* model,
                                     irr::IrrlichtDevice* dev,
                                     std::string snd_address,
                                     std::string snd_port,
                                     std::string rcv_port) {
  this->model = model;
  this->device = dev;

  last_send = 0;
  fresh_cmd = false;

  asio::ip::udp::resolver resolver(io_service);
  asio::ip::udp::resolver::query query(snd_address, snd_port);
  receiver_endpoint = *resolver.resolve(query);
  snd_socket = new asio::ip::udp::socket(io_service);

  terminate_rcv_thread = false;
  std::thread* receive_thread = 0;
  receive_thread =
      new std::thread(&NetworkController::receive_loop, this, rcv_port);
}

NetworkController::~NetworkController() {
  terminate_rcv_mutex.lock();
  terminate_rcv_thread = true;
  terminate_rcv_mutex.unlock();
}

void NetworkController::receive_loop(std::string rcv_port) {
  asio::io_context io_context;
  asio::ip::udp::socket rcv_socket(io_context);

  irr::u16 port = std::stoi(rcv_port);
  rcv_socket.open(asio::ip::udp::v4());
  rcv_socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), port));

  // set up buffer to read data to
  std::string rcv_buffer;
  rcv_buffer.resize(2048);
  for (;;) {
    terminate_rcv_mutex.lock();
    if (terminate_rcv_thread) {
      terminate_rcv_mutex.unlock();
      break;
    }
    terminate_rcv_mutex.unlock();

    // blocking read
    auto nread = rcv_socket.receive(asio::buffer(rcv_buffer));
    if (nread == 0) continue;
    actuator_cmd_mutex.lock();
    try {
      // deserialization
      std::istringstream archive_stream(rcv_buffer);

      boost::archive::text_iarchive archive(archive_stream);
      archive >> actuator_cmd;
      fresh_cmd = true;

      // clear buffer
      for (int i = 0; i <= nread; ++i) {
        rcv_buffer[i] = 0;
      }
    } catch (boost::archive::archive_exception& e) {
      std::cerr
          << "Archive exception while trying to deserialize actuator commands: "
          << e.what() << std::endl;
    }

    actuator_cmd_mutex.unlock();
  }
}

void NetworkController::update_model() {
  if (!fresh_cmd) return;
  actuator_cmd_mutex.lock();
  model->setWheel(actuator_cmd.rudder_angle);
  model->setPortEngine(actuator_cmd.engine_throttle_port);
  model->setStbdEngine(actuator_cmd.engine_throttle_stbd);
  model->setBowThruster(actuator_cmd.thruster_throttle_bow);
  model->setSternThruster(actuator_cmd.thruster_throttle_stern);
  model->setBallastTankPump(actuator_cmd.ballast_tank_pump);
  // clang-format off
  /*
  std::cout << "passing actuator commands to model:" << std::endl;
  std::cout << "    wheel:          " << actuator_cmd.rudder_angle << std::endl;
  std::cout << "    prt engine:     " << actuator_cmd.engine_throttle_port << std::endl;
  std::cout << "    stdb engine:    " << actuator_cmd.engine_throttle_stbd << std::endl;
  std::cout << "    bow thruster:   " << actuator_cmd.thruster_throttle_bow << std::endl;
  std::cout << "    stern thruster: " << actuator_cmd.thruster_throttle_stern << std::endl;
  std::cout << "    ballast pump:   " << actuator_cmd.ballast_tank_pump << std::endl;
  */
  // clang-format on
  actuator_cmd_mutex.unlock();
  fresh_cmd = false;
}

void NetworkController::send_report() {
  irr::u32 now = device->getTimer()->getTime();
  if (now - last_send < SEND_INTERVAL) return;
  // build struct
  irr::f32 depth = model->getDepth();
  irr::f32 lat = model->getLat();
  irr::f32 lon = model->getLong();
  irr::f32 cog = model->getCOG();
  irr::f32 hdt = model->getHeading();
  irr::f32 sog = model->getSOG();
  irr::f32 speed = model->getSpeed();
  irr::f32 rpm_port = model->getPortEngineRPM();
  irr::f32 rpm_stbd = model->getStbdEngineRPM();
  irr::f32 rot = model->getRateOfTurn();
  irr::f32 rudder = model->getRudder();
  irr::f32 throttle_port = model->getPortEngine();
  irr::f32 throttle_stbd = model->getStbdEngine();
  irr::f32 depth_under_keel = model->getDepth();
  irr::f32 ship_depth = model->getShipDepth();
  irr::f32 buoyancy = model->getBuoyancy();

  SensorReport sensors;
  sensors.course_over_ground = cog;
  sensors.depth = depth;
  sensors.gnss_1[0] = model->getNoisyLat();
  sensors.gnss_1[1] = model->getNoisyLong();
  sensors.gnss_2[0] = model->getNoisyLat();
  sensors.gnss_2[1] = model->getNoisyLong();
  sensors.gnss_3[0] = model->getNoisyLat();
  sensors.gnss_3[1] = model->getNoisyLong();
  sensors.heading = hdt;
  sensors.rate_of_turn = rot;
  sensors.rpm_port = rpm_port;
  sensors.rpm_stbd = rpm_stbd;
  sensors.rudder_angle = rudder;
  sensors.speed = speed;
  sensors.speed_over_ground = sog;
  sensors.throttle_port = throttle_port;
  sensors.throttle_stbd = throttle_stbd;
  sensors.depth_under_keel = depth_under_keel;
  sensors.ship_depth = ship_depth;
  sensors.buoyancy = buoyancy;

  // serialize
  std::ostringstream archive_stream;
  boost::archive::text_oarchive archive(archive_stream);
  archive << sensors;

  /*
  std::cout << now << ": sending sensor report" << std::endl;
  std::cout << "lat: " << lat << ", lon: " << lon << std::endl;
  std::cout << "rot: " << rot << ", rudder: " << rudder << std::endl;
  std::cout << "throttle_port: " << throttle_port
            << ", throttle_stbd: " << throttle_stbd << std::endl;
  std::cout << "depth_under_keel: " << depth_under_keel << std::endl;
  std::cout << "ship_depth:" << ship_depth << std::endl;
  */

  if (!snd_socket->is_open()) snd_socket->open(asio::ip::udp::v4());
  snd_socket->send_to(asio::buffer(archive_stream.str()), receiver_endpoint);

  last_send = now;
}
