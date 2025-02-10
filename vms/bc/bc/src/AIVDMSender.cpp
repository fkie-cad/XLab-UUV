#include "./AIVDMSender.hpp"

#include <boost/archive/text_oarchive.hpp>
#include <iomanip>
#include <sstream>

#include "./AIS.hpp"
#include "./BcProxyMessages.hpp"
#include "./NMEA.hpp"

AivdmSender::AivdmSender(SimulationModel *model, irr::IrrlichtDevice *dev,
                         std::string snd_address, std::string snd_port) {
  this->model = model;
  this->device = dev;

  this->messages = std::vector<std::string>();

  asio::ip::udp::resolver resolver(this->io_service);
  asio::ip::udp::resolver::query query(snd_address, snd_port);

  this->receiver_endpoint = *resolver.resolve(query);
  this->snd_socket = new asio::ip::udp::socket(io_service);
}

void AivdmSender::send_aivdm() {
  if (model->getNumberOfOtherShips() <= 0)
    return;

  irr::u32 now = device->getTimer()->getTime();

  std::vector<irr::u32> ready_ships = AIS::getReadyShips(model, now);

  for (auto ship : ready_ships) {
    // generate NMEA AIVDM string
    int fragments = 1;
    int fragment_number = 1;
    char radio_channel = 'B';
    std::string ais_payload;
    int fill_bits;
    std::tie(ais_payload, fill_bits) = AIS::generateClassAReport(model, ship);
    std::ostringstream nmea_s;
    nmea_s << "!AIVDM," << fragments << "," << fragment_number << ","
           << "," << radio_channel << "," << ais_payload << "," << fill_bits;

    // build AivdmMessae struct with unencoded information
    AivdmMessage message;
    message.message = add_nmea_checksum(nmea_s.str());
    // always class A reports
    message.message_type = 1;
    // MMSI of a ship should always exist after at least one
    // call to generateClassAReport
    message.mmsi = model->getOtherShipMMSI(ship);
    irr::f32 sog = model->getOtherShipSpeed(ship);
    message.navigation_status = 0;
    if (sog == 0) {
      message.navigation_status = 1;
    }
    message.latitude = model->getOtherShipLat(ship);
    message.longitude = model->getOtherShipLong(ship);
    // no ROT available
    message.rate_of_turn = 0x80;
    message.speed_over_ground = sog;
    message.course_over_ground = model->getOtherShipHeading(ship);
    message.true_heading = model->getOtherShipHeading(ship);

    // serialize and enqueue
    std::ostringstream archive_stream;
    boost::archive::text_oarchive archive(archive_stream);
    archive << message;

    this->messages.push_back(archive_stream.str());
  }
  ready_ships.clear();

  if (!this->messages.empty()) {
    try {
      if (!this->snd_socket->is_open())
        this->snd_socket->open(asio::ip::udp::v4());
      for (auto message : this->messages) {
        this->snd_socket->send_to(asio::buffer(message), receiver_endpoint);
      }
      this->messages.clear();
    } catch (std::exception &e) {
      device->getLogger()->log(e.what());
    }
  }
}

std::string AivdmSender::add_nmea_checksum(std::string msg) {
  irr::u32 len = msg.length();
  // checksum cannot be a u8 otherwise it will be treated as char by the
  // ostringstream!
  irr::u32 checksum = 0;
  for (int i = 1; i < msg.length(); ++i) {
    checksum ^= msg.at(i);
  }
  std::ostringstream res;
  res << msg << "*" << std::setfill('0') << std::setw(2) << std::uppercase
      << std::hex << checksum << "\r\n";
  return res.str();
}
