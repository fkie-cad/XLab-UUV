#ifndef BC_PROXY_MESSAGES_H
#define BC_PROXY_MESSAGES_H

#include <string>
#include <boost/serialization/string.hpp>

struct SensorReport {
  double course_over_ground;
  double depth;
  double gnss_1[2];
  double gnss_2[2];
  double gnss_3[2];
  double heading;
  double rate_of_turn;
  double rpm_port;
  double rpm_stbd;
  double rudder_angle;
  double speed;
  double speed_over_ground;
  double throttle_port;
  double throttle_stbd;
  double depth_under_keel;
  double ship_depth;
  double buoyancy;

  template <typename Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar& course_over_ground;
    ar& depth;
    ar& gnss_1;
    ar& gnss_2;
    ar& gnss_3;
    ar& heading;
    ar& rate_of_turn;
    ar& rpm_port;
    ar& rpm_stbd;
    ar& rudder_angle;
    ar& speed;
    ar& speed_over_ground;
    ar& throttle_port;
    ar& throttle_stbd;
    ar& depth_under_keel;
    ar& ship_depth;
    ar& buoyancy;
  }
};

struct ActuatorCommands {
  double rudder_angle;
  double engine_throttle_port;
  double engine_throttle_stbd;
  double thruster_throttle_bow;
  double thruster_throttle_stern;
  double ballast_tank_pump;

  template <typename Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar& rudder_angle;
    ar& engine_throttle_port;
    ar& engine_throttle_stbd;
    ar& thruster_throttle_bow;
    ar& thruster_throttle_stern;
    ar& ballast_tank_pump;
  }
};

struct AivdmMessage {
  std::string message;
  uint message_type;
  uint mmsi;
  uint navigation_status;
  double latitude;
  double longitude;
  double rate_of_turn;
  double speed_over_ground;
  double course_over_ground;
  double true_heading;

  template <typename Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar& message;
    ar& message_type;
    ar& mmsi;
    ar& navigation_status;
    ar& latitude;
    ar& longitude;
    ar& rate_of_turn;
    ar& speed_over_ground;
    ar& course_over_ground;
    ar& true_heading;
  }
};

#endif
