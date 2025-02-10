#ifndef MARMATH_H
#define MARMATH_H

#include <cmath>
#include <tuple>
constexpr double EARTH_RADIUS = 6372008.8;
constexpr double DEG_TO_RAD = M_PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / M_PI;
constexpr double MS_TO_KNT = 1.9438444924406045;
constexpr double KNT_TO_MS = 1.0 / MS_TO_KNT;

inline double distance_harvesine(double lat_a, double lon_a, double lat_b,
                                 double lon_b) {
  double phi_a = lat_a * DEG_TO_RAD;
  double phi_b = lat_b * DEG_TO_RAD;
  double lambda_a = lon_a * DEG_TO_RAD;
  double lambda_b = lon_b * DEG_TO_RAD;

  return (2.0 * EARTH_RADIUS *
          asin(sqrt(pow(sin((phi_b - phi_a) / 2.0), 2.0) +
                    cos(phi_a) * cos(phi_b) *
                        pow(sin((lambda_b - lambda_a) / 2.0), 2.0))));
}

inline double relative_bearing(double heading, double lat_own, double lon_own,
                               double lat_other, double lon_other) {
  lat_own = DEG_TO_RAD * lat_own;
  lon_own = DEG_TO_RAD * lon_own;
  lat_other = DEG_TO_RAD * lat_other;
  lon_other = DEG_TO_RAD * lon_other;

  double delta = lon_other - lon_own;

  double theta = atan2(sin(delta) * cos(lat_other),
                       cos(lat_own) * sin(lat_other) -
                           sin(lat_own) * cos(lat_other) * cos(delta));

  return fmod(((theta * 180.0 / M_PI) + 720.0 - heading), 360.0);
}

inline double relative_heading(double heading_own, double heading_other) {
  return std::fmod((720.0 - heading_own + heading_other), 360.0);
}

inline std::tuple<double, double> polar_to_cartesian(double angle,
                                                     double distance) {
  double phi = angle * DEG_TO_RAD;
  return {distance * cos(phi), distance * sin(phi)};
}

inline double shift_long(double lat, double lon, double distance) {
  double phi_a = lat * DEG_TO_RAD;
  double lambda_a = lon * DEG_TO_RAD;

  double lambda_b =
      lambda_a + 2 * asin(distance / (2 * EARTH_RADIUS * cos(phi_a)));
  return lambda_b * RAD_TO_DEG;
}

inline double shift_lat(double lat, double lon, double distance) {
  double phi_a = lat * DEG_TO_RAD;
  double phi_b = phi_a + 2 * asin(distance / (2 * EARTH_RADIUS));
  return phi_b * RAD_TO_DEG;
}

#endif
