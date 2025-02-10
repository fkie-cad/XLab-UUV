"""
This module contains some constants used in conversions between units
or other formulas.

Attributes:
    DEG_TO_RAD: Value to multiply an angle in degrees with to obtain its
        equivalent in radians.
    RAD_TO_DEG: Value to multiply an angle in radians with to obtain its
        equivalent in degrees.
    NM_TO_M: Value to multiply a distance in nautical miles with to obtain
        its equivalent in meters.
    M_TO_NM: Value to multiply a distance in meters with to obtain its
        equivalent in nautical miles.
    MS_TO_KNT: Value to multiply a velocity in meters per second with to
        obtain its equivalent in knots.
    KNT_TO_MS: Value to multiply a velocity in knots with to obtain its.
        equivalent in meters
    EARTH_RADIUS: Arithmetic mean radius of the Earth as defined by the IUGG.
"""

from math import pi

DEG_TO_RAD: float = pi / 180
RAD_TO_DEG: float = 180 / pi

NM_TO_M: float = 1852.0
M_TO_NM: float = 1 / NM_TO_M

MS_TO_KNT: float = 1.9438444924406045
KNT_TO_MS: float = 1 / MS_TO_KNT

# the IUGG defines the arithmetic mean radius to be 6 371.0088 km
EARTH_RADIUS: float = 6371008.8
